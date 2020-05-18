import FWCore.ParameterSet.Config as cms

process = cms.Process("JME")

process.options = cms.untracked.PSet(
        wantSummary = cms.untracked.bool(False),
        allowUnscheduled = cms.untracked.bool(True)
        )

process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_condDBv2_cff')
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_38T_cff')
process.load('FWCore.MessageLogger.MessageLogger_cfi')

process.GlobalTag.globaltag = '102X_upgrade2018_realistic_v19'

process.MessageLogger.cerr.FwkReport.reportEvery = 1000

process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(-1))
process.source = cms.Source("PoolSource",
        fileNames = cms.untracked.vstring(
            '/store/mc/RunIIAutumn18MiniAOD/TTTo2L2Nu_TuneCP5_13TeV-powheg-pythia8/MINIAODSIM/102X_upgrade2018_realistic_v15-v1/270000/0C645070-A197-2648-B6E3-9AA2D7545A4F.root'
            )
        )

events = cms.untracked.VEventRange()
for line in open('TT.txt'):
    events.append(line.rstrip("\n"))
process.source.eventsToProcess = cms.untracked.VEventRange(events)

process.out = cms.OutputModule("PoolOutputModule",
        outputCommands = cms.untracked.vstring('keep *'),
        fileName = cms.untracked.string("jme_reference_sample_mc.root")
        )

# First, apply new JEC over slimmedJets
from PhysicsTools.PatAlgos.producersLayer1.jetUpdater_cff import updatedPatJetCorrFactors
from PhysicsTools.PatAlgos.producersLayer1.jetUpdater_cff import updatedPatJets

process.patJetCorrFactorsReapplyJEC = updatedPatJetCorrFactors.clone(
        src = cms.InputTag("slimmedJets"),
        levels = ['L1FastJet', 'L2Relative', 'L3Absolute'],
        payload = 'AK4PFchs'
        )

process.slimmedJetsNewJEC = updatedPatJets.clone(
        jetSource = cms.InputTag("slimmedJets"),
        jetCorrFactorsSource = cms.VInputTag(cms.InputTag("patJetCorrFactorsReapplyJEC"))
        )

# Second, smear newly corrected jets
process.slimmedJetsSmeared = cms.EDProducer('SmearedPATJetProducer',
        src = cms.InputTag('slimmedJetsNewJEC'),
        enabled = cms.bool(True),
        rho = cms.InputTag("fixedGridRhoFastjetAll"),
        algo = cms.string('AK4PFchs'),
        algopt = cms.string('AK4PFchs_pt'),
        genJets = cms.InputTag('slimmedGenJets'),
        dRMax = cms.double(0.2),
        dPtMaxFactor = cms.double(3),
        debug = cms.untracked.bool(False)
        )

# MET
process.genMet = cms.EDProducer("GenMETExtractor",
        metSource = cms.InputTag("slimmedMETs", "", "@skipCurrentProcess")
        )

# Raw MET
process.uncorrectedMet = cms.EDProducer("RecoMETExtractor",
        correctionLevel = cms.string('raw'),
        metSource = cms.InputTag("slimmedMETs", "", "@skipCurrentProcess")
        )

# Raw PAT MET
from PhysicsTools.PatAlgos.tools.metTools import addMETCollection
addMETCollection(process, labelName="uncorrectedPatMet", metSource="uncorrectedMet")
process.uncorrectedPatMet.genMETSource = cms.InputTag('genMet')

# Type-1 correction
process.Type1CorrForNewJEC = cms.EDProducer("PATPFJetMETcorrInputProducer",
        src = cms.InputTag("slimmedJetsNewJEC"),
        jetCorrLabel = cms.InputTag("L3Absolute"),
        jetCorrLabelRes = cms.InputTag("L2L3Residual"),
        offsetCorrLabel = cms.InputTag("L1FastJet"),
        skipEM = cms.bool(True),
        skipEMfractionThreshold = cms.double(0.9),
        skipMuonSelection = cms.string('isGlobalMuon | isStandAloneMuon'),
        skipMuons = cms.bool(True),
        type1JetPtThreshold = cms.double(15.0)
        )

process.slimmedMETsNewJEC = cms.EDProducer('CorrectedPATMETProducer',
        src = cms.InputTag('uncorrectedPatMet'),
        srcCorrections = cms.VInputTag(cms.InputTag('Type1CorrForNewJEC', 'type1'))
        )

process.shiftedMETCorrModuleForSmearedJets = cms.EDProducer('ShiftedParticleMETcorrInputProducer',
        srcOriginal = cms.InputTag('slimmedJetsNewJEC'),
        srcShifted = cms.InputTag('slimmedJetsSmeared')
        )

process.slimmedMETsSmeared = cms.EDProducer('CorrectedPATMETProducer',
        src = cms.InputTag('slimmedMETsNewJEC'),
        srcCorrections = cms.VInputTag(cms.InputTag('shiftedMETCorrModuleForSmearedJets'))
        )

process.produceTable = cms.EDAnalyzer('JMEReferenceTableAnalyzer',
        plain_jets = cms.InputTag('slimmedJets'),
        recorrected_jets = cms.InputTag('slimmedJetsNewJEC'),
        smeared_jets = cms.InputTag('slimmedJetsSmeared'),

        plain_met = cms.InputTag('slimmedMETs'),
        recorrected_met = cms.InputTag('slimmedMETsNewJEC'),
        smeared_met = cms.InputTag('slimmedMETsSmeared')
        )


process.p = cms.Path(process.patJetCorrFactorsReapplyJEC + process.slimmedJetsNewJEC + process.slimmedJetsSmeared + process.genMet + process.uncorrectedMet + process.uncorrectedPatMet + process.Type1CorrForNewJEC + process.slimmedMETsNewJEC + process.shiftedMETCorrModuleForSmearedJets + process.slimmedMETsSmeared + process.produceTable)

#process.p = cms.Path(process.produceTable)
process.end = cms.EndPath(process.out)

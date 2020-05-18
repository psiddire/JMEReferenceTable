// -*- C++ -*-
//
// Package:    JetMETCorrections/JMEReferenceTableAnalyzer
// Class:      JMEReferenceTableAnalyzer
// 
/**\class JMEReferenceTableAnalyzer JMEReferenceTableAnalyzer.cc JetMETCorrections/JMEReferenceTableAnalyzer/plugins/JMEReferenceTableAnalyzer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Sebastien Brochet
//         Created:  Thu, 25 Feb 2016 17:05:48 GMT
//
//


// system include files
#include <memory>

// user include files
#include "CommonTools/UtilAlgos/interface/DeltaR.h"

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "FWCore/Utilities/interface/InputTag.h"

#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/PatCandidates/interface/MET.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "JetMETCorrections/Objects/interface/JetCorrectionsRecord.h"
#include "CondFormats/JetMETObjects/interface/JetCorrectionUncertainty.h"
#include "CondFormats/JetMETObjects/interface/JetCorrectorParameters.h"

//
// class declaration
//

// If the analyzer does not use TFileService, please remove
// the template argument to the base class so the class inherits
// from  edm::one::EDAnalyzer<> and also remove the line from
// constructor "usesResource("TFileService");"
// This will improve performance in multithreaded jobs.

class JMEReferenceTableAnalyzer : public edm::one::EDAnalyzer<>  {
   public:
      explicit JMEReferenceTableAnalyzer(const edm::ParameterSet&);
      ~JMEReferenceTableAnalyzer();

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);


   private:
      virtual void beginJob() override;
      virtual void analyze(const edm::Event&, const edm::EventSetup&) override;
      virtual void endJob() override;

      // ----------member data ---------------------------
      edm::EDGetTokenT<edm::View<pat::Jet>> miniaod_jets_token;
      edm::EDGetTokenT<edm::View<pat::Jet>> recorrected_jets_token;
      edm::EDGetTokenT<edm::View<pat::Jet>> smeared_jets_token;

      edm::EDGetTokenT<pat::METCollection> miniaod_met_token;
      edm::EDGetTokenT<pat::METCollection> recorrected_met_token;
      edm::EDGetTokenT<pat::METCollection> smeared_met_token;

      std::unique_ptr<JetCorrectionUncertainty> jetUncertainty;

      std::ostringstream table;
};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
JMEReferenceTableAnalyzer::JMEReferenceTableAnalyzer(const edm::ParameterSet& iConfig)
{
    miniaod_jets_token = consumes<edm::View<pat::Jet>>(iConfig.getParameter<edm::InputTag>("plain_jets"));
    recorrected_jets_token = consumes<edm::View<pat::Jet>>(iConfig.getParameter<edm::InputTag>("recorrected_jets"));
    smeared_jets_token = consumes<edm::View<pat::Jet>>(iConfig.getParameter<edm::InputTag>("smeared_jets"));

    miniaod_met_token = consumes<pat::METCollection>(iConfig.getParameter<edm::InputTag>("plain_met"));
    recorrected_met_token = consumes<pat::METCollection>(iConfig.getParameter<edm::InputTag>("recorrected_met"));
    smeared_met_token = consumes<pat::METCollection>(iConfig.getParameter<edm::InputTag>("smeared_met"));
}


JMEReferenceTableAnalyzer::~JMEReferenceTableAnalyzer()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)
    std::cout << table.str() << std::endl;

}

template<typename T, typename U> const T& getMatch(const T& object, const U& collection) {

    for (const auto& o: collection) {
        if (deltaR(object, o) < 1e-6)
            return o;        
    }

    throw std::runtime_error("No match found");
}

//
// member functions
//

// ------------ method called for each event  ------------
void
JMEReferenceTableAnalyzer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
    edm::Handle<edm::View<pat::Jet>> miniaod_jets_handle;
    iEvent.getByToken(miniaod_jets_token, miniaod_jets_handle);

    edm::Handle<edm::View<pat::Jet>> recorrected_jets_handle;
    iEvent.getByToken(recorrected_jets_token, recorrected_jets_handle);

    edm::Handle<edm::View<pat::Jet>> smeared_jets_handle;
    iEvent.getByToken(smeared_jets_token, smeared_jets_handle);

    edm::Handle<pat::METCollection> miniaod_met_handle;
    iEvent.getByToken(miniaod_met_token, miniaod_met_handle);

    edm::Handle<pat::METCollection> recorrected_met_handle;
    iEvent.getByToken(recorrected_met_token, recorrected_met_handle);

    edm::Handle<pat::METCollection> smeared_met_handle;
    iEvent.getByToken(smeared_met_token, smeared_met_handle);

    if (! jetUncertainty) {
        edm::ESHandle<JetCorrectorParametersCollection> jetCorrParameterSet;
        iSetup.get<JetCorrectionsRecord>().get("AK4PFchs", jetCorrParameterSet);
        const JetCorrectorParameters& jetCorrParameters = (*jetCorrParameterSet)["Uncertainty"];
        jetUncertainty.reset(new JetCorrectionUncertainty(jetCorrParameters));
    }

    // Table header
    table << R"(|  *Event*  |  *Jet #*  |  *\(p_T\)*  |  *\(\eta\)*  |  *Uncorrected \(p_T\)*  |  *Recorrected \(p_T\) (using new JEC)*  |  *Smeared \(p_T\) (jet smeared after new JEC)*  |  *JEC uncertainty (%)*  |)" << std::endl;


    size_t j = 1;
    for (const auto& jet: *miniaod_jets_handle) {
        if (j == 1) {
            table << "|  " << iEvent.id().run() << ":" << iEvent.id().luminosityBlock() << ":" << iEvent.id().event() << "  |  ";
        } else {
            table << "|^|  ";
        }
        table << "#" << j << "  |  " << jet.pt() << "  |  " << jet.eta() << "  |  " << jet.correctedP4("Uncorrected").pt();
        j++;

        // Look for corrected jet and smeared jet
        const pat::Jet& recorrectedJet = getMatch(jet, *recorrected_jets_handle);
        const pat::Jet& smearedJet = getMatch(jet, *smeared_jets_handle);

        jetUncertainty->setJetEta(recorrectedJet.eta());
        jetUncertainty->setJetPt(recorrectedJet.pt());
        table << "  |  " << recorrectedJet.pt() << "  |  " << smearedJet.pt() << "  |  " << jetUncertainty->getUncertainty(true) * 100 << "  |" << std::endl;
    }

    // MET
    table << R"(|  *Event*  | |  *MET \(p_T\) (with Type-1)*  |  *MET \(\varphi\)*  |  *MET Uncorrected \(p_T\)*  |  *Recorrected MET \(p_T\)*  |  *Smeared MET \(p_T\)*  |  |)" << std::endl;

    table << "|  " << iEvent.id().run() << ":" << iEvent.id().luminosityBlock() << ":" << iEvent.id().event() << "  |  ";
    table << "  |  " << miniaod_met_handle->front().pt() << "  |  " << miniaod_met_handle->front().phi() << "  |  " << miniaod_met_handle->front().uncorPt() << "  |  " << recorrected_met_handle->front().pt() << "  |  " << smeared_met_handle->front().pt() << "  |  |" << std::endl;

    table << std::endl;
}


// ------------ method called once each job just before starting event loop  ------------
void 
JMEReferenceTableAnalyzer::beginJob()
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
JMEReferenceTableAnalyzer::endJob() 
{
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
JMEReferenceTableAnalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

// Define this as a plug-in
DEFINE_FWK_MODULE(JMEReferenceTableAnalyzer);

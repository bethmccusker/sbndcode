///////////////////////////////////////////////////////////////////////////////
/// Class: CRTDetSim
/// Module Type: producer
/// File: CRTDetSim_module.cc
///
/// Based on LArIAT TOFSimDigits.cc (Author: Lucas Mendes Santos)
///
/// Author: mastbaum@uchicago.edu
///////////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardata/DetectorInfoServices/DetectorClocksServiceStandard.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/AuxDetGeo.h"
#include "larcore/Geometry/AuxDetGeometry.h"
#include "larsim/Simulation/AuxDetSimChannel.h"

#include "TRandom.h"
#include "CRTData.hh"

#include <cmath>
#include <memory>

namespace crt {

class CRTDetSim : public art::EDProducer {
public:
  explicit CRTDetSim(fhicl::ParameterSet const & p);

  CRTDetSim(CRTDetSim const &) = delete;
  CRTDetSim(CRTDetSim &&) = delete;
  CRTDetSim& operator = (CRTDetSim const &) = delete;
  CRTDetSim& operator = (CRTDetSim &&) = delete;
  void reconfigure(fhicl::ParameterSet const & p) override;

  void produce(art::Event & e) override;
  std::string fG4ModuleLabel;
};


void CRTDetSim::reconfigure(fhicl::ParameterSet const & p) {
  fG4ModuleLabel = p.get<std::string>("G4ModuleLabel");
}


CRTDetSim::CRTDetSim(fhicl::ParameterSet const & p) {
  this->reconfigure(p);
  produces<std::vector<crt::CRTData> >();
}


void CRTDetSim::produce(art::Event & e) {
  std::unique_ptr<std::vector<crt::CRTData> > crtHits(
      new std::vector<crt::CRTData>);

  art::ServiceHandle<geo::Geometry> geoService;

  art::Handle<std::vector<sim::AuxDetSimChannel> > channels;

  e.getByLabel(fG4ModuleLabel, channels);

  for (auto& adsc : *channels) {
    const geo::AuxDetGeo& adGeo = \
        geoService->AuxDet(adsc.AuxDetID());

    const geo::AuxDetSensitiveGeo& adsGeo = \
        adGeo.SensitiveVolume(adsc.AuxDetSensitiveID());

    std::string moduleName = adGeo.TotalVolume()->GetName();  // Strip array
    std::cout << "n "
              << adGeo.TotalVolume()->GetNtotal() << " / "
              << adsGeo.TotalVolume()->GetNtotal() << std::endl;

    for (auto ide : adsc.AuxDetIDEs()) {
      // Number adjacent channels on a strip sequentially
      uint32_t stripID = adsc.AuxDetSensitiveID();
      uint32_t channel0ID = 2 * stripID + 0;
      uint32_t channel1ID = 2 * stripID + 1;

      // Simulate the CRT response
      // Distance from the hit centroid to the readout end. Which way is
      // "outside" depends on which module was hit.
      double x = (ide.entryX + ide.exitX) / 2;
      double y = (ide.entryY + ide.exitY) / 2;
      double z = (ide.entryZ + ide.exitZ) / 2;
      double world[3] = {x, y, z};


      // Hit position in strip's local coordinates
      double svHitPosLocal[3];
      adsGeo.WorldToLocal(world, svHitPosLocal);

      // Center of the module in local coordinates
      double svCenterPosWorld[3];
      adsGeo.GetCenter(svCenterPosWorld);
      double svCenterPosLocal[3];
      adGeo.WorldToLocal(svCenterPosWorld, svCenterPosLocal);

      // Distance to the readout end ("outside") depends on module position
      double distToReadout = -1;
      if (svCenterPosLocal[1] > 0) {
        distToReadout = abs(0 - adsGeo.HalfHeight());
      }
      else {
        distToReadout = abs(0 + adsGeo.HalfHeight());
      }

      // The expected number of PE for relativistic muons, based on an
      // exponential fit to the data in Figure 4.4 of SBND Document 685-v5
      int npeExpected = 43.62 * exp(-distToReadout/652.51);
      int npeTruth = gRandom->Poisson(npeExpected);

      // Gaussian smearing for resolution
      double npe = gRandom->Gaus(npeTruth, 0.25);

      // Made-up "Digitizer" cf. SBND Document 685-v5, Figure 3.8
      short q = 100.0 * (1.0 * npe);

      // Write AuxDetDigit for each channel (currently the same waveform)
      crtHits->push_back(::crt::CRTData(channel0ID, 21, 42, q));
      crtHits->push_back(::crt::CRTData(channel1ID, 21, 42, q));
    }
  }

  e.put(std::move(crtHits));
}

DEFINE_ART_MODULE(CRTDetSim)

}  // namespace crt


#include <TGeoVolume.h>
#include <TGeoTube.h>
#include <TGeoManager.h>
#include "AliITSURecoDet.h"
#include "AliITSUGeomTGeo.h"
#include "AliITSsegmentation.h"
#include "AliITSUSegmentationPix.h"
#include "AliITSUReconstructor.h"


ClassImp(AliITSURecoDet)


const Char_t* AliITSURecoDet::fgkBeamPipeVolName = "IP_PIPE";


//______________________________________________________
AliITSURecoDet::AliITSURecoDet(AliITSUGeomTGeo* geom, const char* name)
:  fNLayers(0)
  ,fNLayersActive(0)
  ,fRMax(-1)
  ,fRMin(-1)
  ,fLayers(0)
  ,fLayersActive(0)
  ,fGeom(geom)
{
  // def. c-tor
  SetNameTitle(name,name);
  fLayers.SetOwner(kTRUE);        // layers belong to this array
  fLayersActive.SetOwner(kFALSE); // this one just points on active layers in fLayers
  Build();
}

//______________________________________________________
AliITSURecoDet::~AliITSURecoDet()
{
  // def. d-tor
  fLayersActive.Clear(); 
  fLayers.Clear();         // owned!
}

//______________________________________________________
void AliITSURecoDet::Print(Option_t* opt) const			      
{
  //print 
  printf("Detector %s, %d layers, %d active layers\n",GetName(),GetNLayers(),GetNLayersActive());
  TString opts = opt; opts.ToLower();
  if (opts.Contains("lr")) for (int i=0;i<GetNLayers();i++) GetLayer(i)->Print(opt);
}

//______________________________________________________
void AliITSURecoDet::AddLayer(const AliITSURecoLayer* lr)
{
  //add new layer
  fLayers.AddLast((TObject*)lr);
  fNLayers++;
  if (lr->IsActive()) {
    fLayersActive.AddLast((TObject*)lr);
    fNLayersActive++;
  }
}

//______________________________________________________
Bool_t AliITSURecoDet::Build()
{
  // build detector from TGeo
  //
  if (!fGeom) AliFatal("Geometry interface is not set");
  int nlr = fGeom->GetNLayers();
  if (!nlr) AliFatal("No geometry loaded");
  //
  // build active ITS layers
  for (int ilr=0;ilr<nlr;ilr++) {
    int lrTyp = fGeom->GetLayerDetTypeID(ilr);
    // name layer according its active id, detector type and segmentation tyoe
    AliITSURecoLayer* lra = new AliITSURecoLayer(Form("Lr%d%s%d",ilr,fGeom->GetDetTypeName(lrTyp),
						      lrTyp%AliITSUGeomTGeo::kMaxSegmPerDetType),
						 ilr,fGeom);
    lra->SetPassive(kFALSE);
    AddLayer(lra);
  }
  //
  // build passive ITS layers
  //
  double rMin,rMax,zMin,zMax;
  // beam pipe
  TGeoVolume *v = gGeoManager->GetVolume(fgkBeamPipeVolName);
  AliITSURecoLayer* lrp = 0;
  if (!v) AliWarning("No beam pipe found in geometry");
  {
    TGeoTube *t=(TGeoTube*)v->GetShape();
    rMin = t->GetRmin();
    rMax = t->GetRmax();
    zMin =-t->GetDz();
    zMax = t->GetDz();
    lrp = new AliITSURecoLayer("BeamPipe");
    lrp->SetRMin(rMin);
    lrp->SetRMax(rMax);
    lrp->SetR(0.5*(rMin+rMax));
    lrp->SetZMin(zMin);
    lrp->SetZMax(zMax);
    lrp->SetPassive(kTRUE);
    AddLayer(lrp);
    //
  }
  //
  // TPC-ITS wall
  lrp = new AliITSURecoLayer("TPC-ITSwall");
  lrp->SetRMin(AliITSUReconstructor::GetRecoParam()->GetTPCITSWallRMin());
  lrp->SetRMax(AliITSUReconstructor::GetRecoParam()->GetTPCITSWallRMax());
  lrp->SetR(0.5*(lrp->GetRMin()+lrp->GetRMax()));
  lrp->SetZMin(-AliITSUReconstructor::GetRecoParam()->GetTPCITSWallZSpanH());
  lrp->SetZMax( AliITSUReconstructor::GetRecoParam()->GetTPCITSWallZSpanH());
  lrp->SetMaxStep( AliITSUReconstructor::GetRecoParam()->GetTPCITSWallMaxStep());
  lrp->SetPassive(kTRUE);
  AddLayer(lrp);
  //
  IndexLayers();
  Print("lr");
  return kTRUE;
}

//______________________________________________________
void AliITSURecoDet::IndexLayers()
{
  fLayersActive.Sort();
  for (int i=0;i<fNLayersActive;i++) GetLayerActive(i)->SetActiveID(i);
  fLayers.Sort();
  for (int i=0;i<fNLayers;i++) GetLayer(i)->SetID(i);
}
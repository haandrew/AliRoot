/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

/*
$Log$
Revision 1.27  2003/02/25 16:47:58  hristov
allow back propagation for more than 1 event (J.Chudoba)

Revision 1.26  2003/02/19 08:49:46  hristov
Track time measurement (S.Radomski)

Revision 1.25  2003/01/28 16:43:35  hristov
Additional protection: to be discussed with the Root team (M.Ivanov)

Revision 1.24  2002/11/19 16:13:24  hristov
stdlib.h included to declare exit() on HP

Revision 1.23  2002/11/19 11:50:08  hristov
Removing CONTAINERS (Yu.Belikov)

Revision 1.19  2002/07/19 07:31:40  kowal2
Improvement in tracking by J. Belikov

Revision 1.18  2002/05/13 07:33:52  kowal2
Added protection in Int_t AliTPCtracker::AliTPCRow::Find(Double_t y) const
in the case of defined region of interests.

Revision 1.17  2002/03/18 17:59:13  kowal2
Chnges in the pad geometry - 3 pad lengths introduced.

Revision 1.16  2001/11/08 16:39:03  hristov
Additional protection (M.Masera)

Revision 1.15  2001/11/08 16:36:33  hristov
Updated V2 stream of tracking (Yu.Belikov). The new long waited features are: 1) Possibility to pass the primary vertex position to the trackers (both for the TPC and the ITS) 2) Possibility to specify the number of tracking passes together with applying (or not applying) the vertex constraint (ITS only) 3) Possibility to make some use of partial PID provided by the TPC when doing tracking in the ITS (ITS only) 4) V0 reconstruction with a helix minimisation of the DCA. (new macros: AliV0FindVertices.C and AliV0Comparison.C) 4a) ( Consequence of the 4) )  All the efficiencies and resolutions are from now on calculated including *secondary*particles* too. (Don't be surprised by the drop in efficiency etc)

Revision 1.14  2001/10/21 19:04:55  hristov
Several patches were done to adapt the barel reconstruction to the multi-event case. Some memory leaks were corrected. (Yu.Belikov)

Revision 1.13  2001/07/23 12:01:30  hristov
Initialisation added

Revision 1.12  2001/07/20 14:32:44  kowal2
Processing of many events possible now

Revision 1.11  2001/05/23 08:50:10  hristov
Weird inline removed

Revision 1.10  2001/05/16 14:57:25  alibrary
New files for folders and Stack

Revision 1.9  2001/05/11 07:16:56  hristov
Fix needed on Sun and Alpha

Revision 1.8  2001/05/08 15:00:15  hristov
Corrections for tracking in arbitrary magnenetic field. Changes towards a concept of global Alice track. Back propagation of reconstructed tracks (Yu.Belikov)

Revision 1.5  2000/12/20 07:51:59  kowal2
Changes suggested by Alessandra and Paolo to avoid overlapped
data fields in encapsulated classes.

Revision 1.4  2000/11/02 07:27:16  kowal2
code corrections

Revision 1.2  2000/06/30 12:07:50  kowal2
Updated from the TPC-PreRelease branch

Revision 1.1.2.1  2000/06/25 08:53:55  kowal2
Splitted from AliTPCtracking

*/

//-------------------------------------------------------
//          Implementation of the TPC tracker
//
//   Origin: Iouri Belikov, CERN, Jouri.Belikov@cern.ch 
//-------------------------------------------------------
#include <TObjArray.h>
#include <TFile.h>
#include <TTree.h>
#include <Riostream.h>

#include "AliTPCtracker.h"
#include "AliTPCcluster.h"
#include "AliTPCParam.h"
#include "AliClusters.h"

#include "TVector2.h"

#include <stdlib.h>
//_____________________________________________________________________________
AliTPCtracker::AliTPCtracker(const AliTPCParam *par): 
AliTracker(), fkNIS(par->GetNInnerSector()/2), fkNOS(par->GetNOuterSector()/2)
{
  //---------------------------------------------------------------------
  // The main TPC tracker constructor
  //---------------------------------------------------------------------
  fInnerSec=new AliTPCSector[fkNIS];         
  fOuterSec=new AliTPCSector[fkNOS];

  Int_t i;
  for (i=0; i<fkNIS; i++) fInnerSec[i].Setup(par,0);
  for (i=0; i<fkNOS; i++) fOuterSec[i].Setup(par,1);

  fParam = (AliTPCParam*) par;
  fSeeds=0;
}

//_____________________________________________________________________________
AliTPCtracker::~AliTPCtracker() {
  //------------------------------------------------------------------
  // TPC tracker destructor
  //------------------------------------------------------------------
  delete[] fInnerSec;
  delete[] fOuterSec;
  if (fSeeds) {
    fSeeds->Delete(); 
    delete fSeeds;
  }
}

//_____________________________________________________________________________
Double_t SigmaY2(Double_t r, Double_t tgl, Double_t pt)
{
  //
  // Parametrised error of the cluster reconstruction (pad direction)   
  //
  // Sigma rphi
  const Float_t kArphi=0.41818e-2;
  const Float_t kBrphi=0.17460e-4;
  const Float_t kCrphi=0.30993e-2;
  const Float_t kDrphi=0.41061e-3;
  
  pt=TMath::Abs(pt)*1000.;
  Double_t x=r/pt;
  tgl=TMath::Abs(tgl);
  Double_t s=kArphi - kBrphi*r*tgl + kCrphi*x*x + kDrphi*x;
  if (s<0.4e-3) s=0.4e-3;
  s*=1.3; //Iouri Belikov

  return s;
}

//_____________________________________________________________________________
Double_t SigmaZ2(Double_t r, Double_t tgl) 
{
  //
  // Parametrised error of the cluster reconstruction (drift direction)
  //
  // Sigma z
  const Float_t kAz=0.39614e-2;
  const Float_t kBz=0.22443e-4;
  const Float_t kCz=0.51504e-1;
  

  tgl=TMath::Abs(tgl);
  Double_t s=kAz - kBz*r*tgl + kCz*tgl*tgl;
  if (s<0.4e-3) s=0.4e-3;
  s*=1.3; //Iouri Belikov

  return s;
}

//_____________________________________________________________________________
Double_t f1(Double_t x1,Double_t y1,
                   Double_t x2,Double_t y2,
                   Double_t x3,Double_t y3) 
{
  //-----------------------------------------------------------------
  // Initial approximation of the track curvature
  //-----------------------------------------------------------------
  Double_t d=(x2-x1)*(y3-y2)-(x3-x2)*(y2-y1);
  Double_t a=0.5*((y3-y2)*(y2*y2-y1*y1+x2*x2-x1*x1)-
                  (y2-y1)*(y3*y3-y2*y2+x3*x3-x2*x2));
  Double_t b=0.5*((x2-x1)*(y3*y3-y2*y2+x3*x3-x2*x2)-
                  (x3-x2)*(y2*y2-y1*y1+x2*x2-x1*x1));

  Double_t xr=TMath::Abs(d/(d*x1-a)), yr=d/(d*y1-b);

  return -xr*yr/sqrt(xr*xr+yr*yr); 
}


//_____________________________________________________________________________
Double_t f2(Double_t x1,Double_t y1,
                   Double_t x2,Double_t y2,
                   Double_t x3,Double_t y3) 
{
  //-----------------------------------------------------------------
  // Initial approximation of the track curvature times center of curvature
  //-----------------------------------------------------------------
  Double_t d=(x2-x1)*(y3-y2)-(x3-x2)*(y2-y1);
  Double_t a=0.5*((y3-y2)*(y2*y2-y1*y1+x2*x2-x1*x1)-
                  (y2-y1)*(y3*y3-y2*y2+x3*x3-x2*x2));
  Double_t b=0.5*((x2-x1)*(y3*y3-y2*y2+x3*x3-x2*x2)-
                  (x3-x2)*(y2*y2-y1*y1+x2*x2-x1*x1));

  Double_t xr=TMath::Abs(d/(d*x1-a)), yr=d/(d*y1-b);
  
  return -a/(d*y1-b)*xr/sqrt(xr*xr+yr*yr);
}

//_____________________________________________________________________________
Double_t f3(Double_t x1,Double_t y1, 
                   Double_t x2,Double_t y2,
                   Double_t z1,Double_t z2) 
{
  //-----------------------------------------------------------------
  // Initial approximation of the tangent of the track dip angle
  //-----------------------------------------------------------------
  return (z1 - z2)/sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

//_____________________________________________________________________________
void AliTPCtracker::LoadClusters() {
  //-----------------------------------------------------------------
  // This function loads TPC clusters.
  //-----------------------------------------------------------------
  if (!gFile->IsOpen()) {
    cerr<<"AliTPCtracker::LoadClusters : "<<
      "file with clusters has not been open !\n";
    return;
  }

  Char_t name[100];
  sprintf(name,"TreeC_TPC_%d",GetEventNumber());
  TTree *cTree=(TTree*)gFile->Get(name);
  if (!cTree) {
    cerr<<"AliTPCtracker::LoadClusters : "<<
      "can't get the tree with TPC clusters !\n";
    return;
  }

  TBranch *branch=cTree->GetBranch("Segment");
  if (!branch) {
    cerr<<"AliTPCtracker::LoadClusters : "<<
      "can't get the segment branch !\n";
    return;
  }
  AliClusters carray, *addr=&carray;
  carray.SetClass("AliTPCcluster");
  carray.SetArray(0);
  branch->SetAddress(&addr);

  Int_t nentr=(Int_t)cTree->GetEntries();

  for (Int_t i=0; i<nentr; i++) {
      cTree->GetEvent(i);

      Int_t ncl=carray.GetArray()->GetEntriesFast();

      Int_t nir=fInnerSec->GetNRows(), nor=fOuterSec->GetNRows();
      Int_t id=carray.GetID();
      if ((id<0) || (id>2*(fkNIS*nir + fkNOS*nor))) {
         cerr<<"AliTPCtracker::LoadClusters : "<<
	       "wrong index !\n";
         exit(1);
      }        
      Int_t outindex = 2*fkNIS*nir;
      if (id<outindex) {
         Int_t sec = id/nir;
         Int_t row = id - sec*nir;
         sec %= fkNIS;
         AliTPCRow &padrow=fInnerSec[sec][row];
         while (ncl--) {
           AliTPCcluster *c=(AliTPCcluster*)carray[ncl];
           padrow.InsertCluster(c,sec,row);
         }           
      } else {
         id -= outindex;
         Int_t sec = id/nor;
         Int_t row = id - sec*nor;
         sec %= fkNOS;
         AliTPCRow &padrow=fOuterSec[sec][row];
         while (ncl--) {
           AliTPCcluster *c=(AliTPCcluster*)carray[ncl];
           padrow.InsertCluster(c,sec+fkNIS,row);
         }
      }

      carray.GetArray()->Clear();
  }
  delete cTree;
}

//_____________________________________________________________________________
void AliTPCtracker::UnloadClusters() {
  //-----------------------------------------------------------------
  // This function unloads TPC clusters.
  //-----------------------------------------------------------------
  Int_t i;
  for (i=0; i<fkNIS; i++) {
    Int_t nr=fInnerSec->GetNRows();
    for (Int_t n=0; n<nr; n++) fInnerSec[i][n].ResetClusters();
  }
  for (i=0; i<fkNOS; i++) {
    Int_t nr=fOuterSec->GetNRows();
    for (Int_t n=0; n<nr; n++) fOuterSec[i][n].ResetClusters();
  }
}

//_____________________________________________________________________________
Int_t AliTPCtracker::FollowProlongation(AliTPCseed& t, Int_t rf) {
  //-----------------------------------------------------------------
  // This function tries to find a track prolongation.
  //-----------------------------------------------------------------
  Double_t xt=t.GetX();
  const Int_t kSKIP=(t.GetNumberOfClusters()<10) ? kRowsToSkip : 
                    Int_t(0.5*fSectors->GetNRows());
  Int_t tryAgain=kSKIP;

  Double_t alpha=t.GetAlpha() - fSectors->GetAlphaShift();
  if (alpha > 2.*TMath::Pi()) alpha -= 2.*TMath::Pi();  
  if (alpha < 0.            ) alpha += 2.*TMath::Pi();  
  Int_t s=Int_t(alpha/fSectors->GetAlpha())%fN;

  Int_t nrows=fSectors->GetRowNumber(xt)-1;
  for (Int_t nr=nrows; nr>=rf; nr--) {
    Double_t x=fSectors->GetX(nr), ymax=fSectors->GetMaxY(nr);
    if (!t.PropagateTo(x)) return 0;

    AliTPCcluster *cl=0;
    UInt_t index=0;
    Double_t maxchi2=kMaxCHI2;
    const AliTPCRow &krow=fSectors[s][nr];
    Double_t pt=t.GetConvConst()/(100/0.299792458/0.2)/t.Get1Pt();
    Double_t sy2=SigmaY2(t.GetX(),t.GetTgl(),pt);
    Double_t sz2=SigmaZ2(t.GetX(),t.GetTgl());
    Double_t road=4.*sqrt(t.GetSigmaY2() + sy2), y=t.GetY(), z=t.GetZ();

    if (road>kMaxROAD) {
      if (t.GetNumberOfClusters()>4) 
        cerr<<t.GetNumberOfClusters()
        <<"FindProlongation warning: Too broad road !\n"; 
      return 0;
    }

    if (krow) {
      for (Int_t i=krow.Find(y-road); i<krow; i++) {
	AliTPCcluster *c=(AliTPCcluster*)(krow[i]);
	if (c->GetY() > y+road) break;
	if (c->IsUsed()) continue;
	if ((c->GetZ()-z)*(c->GetZ()-z) > 16.*(t.GetSigmaZ2()+sz2)) continue;
	Double_t chi2=t.GetPredictedChi2(c);
	if (chi2 > maxchi2) continue;
	maxchi2=chi2;
	cl=c;
        index=krow.GetIndex(i);       
      }
    }
    if (cl) {
      Float_t l=fSectors->GetPadPitchWidth();
      Float_t corr=1.; if (nr>63) corr=0.67; // new (third) pad response !
      t.SetSampledEdx(cl->GetQ()/l*corr,t.GetNumberOfClusters());
      if (!t.Update(cl,maxchi2,index)) {
         if (!tryAgain--) return 0;
      } else tryAgain=kSKIP;
    } else {
      if (tryAgain==0) break;
      if (y > ymax) {
         s = (s+1) % fN;
         if (!t.Rotate(fSectors->GetAlpha())) return 0;
      } else if (y <-ymax) {
         s = (s-1+fN) % fN;
         if (!t.Rotate(-fSectors->GetAlpha())) return 0;
      }
      tryAgain--;
    }
  }

  return 1;
}
//_____________________________________________________________________________

Int_t AliTPCtracker::FollowRefitInward(AliTPCseed *seed, AliTPCtrack *track) {
  //
  // This function propagates seed inward TPC using old clusters
  // from track.
  // 
  // Sylwester Radomski, GSI
  // 26.02.2003
  //

  Double_t alpha=seed->GetAlpha() - fSectors->GetAlphaShift();
  TVector2::Phi_0_2pi(alpha);
  Int_t s=Int_t(alpha/fSectors->GetAlpha())%fN;

  Int_t idx=-1, sec=-1, row=-1;
  Int_t nc = seed->GetLabel(); // index to start with

  idx=track->GetClusterIndex(nc);
  sec=(idx&0xff000000)>>24; 
  row=(idx&0x00ff0000)>>16;
  
  if (fSectors==fInnerSec) { if (sec >= fkNIS) row=-1; } 
  else { if (sec <  fkNIS) row=-1; }   

  Int_t nr=fSectors->GetNRows();
  for (Int_t i=0; i<nr; i++) {

    Double_t x=fSectors->GetX(i); 
    if (!seed->PropagateTo(x)) return 0;

    // Change sector and rotate seed if necessary

    Double_t ymax=fSectors->GetMaxY(i);
    Double_t y=seed->GetY();

    if (y > ymax) {
       s = (s+1) % fN;
       if (!seed->Rotate(fSectors->GetAlpha())) return 0;
    } else if (y <-ymax) {
       s = (s-1+fN) % fN;
       if (!seed->Rotate(-fSectors->GetAlpha())) return 0;
    }

    // try to find a cluster
    
    AliTPCcluster *cl=0;
    Int_t index = 0;
    Double_t maxchi2 = kMaxCHI2;
    
    //cout << i << " " << row << " " << nc << endl;

    if (row==i) {

      // accept already found cluster
      AliTPCcluster *c=(AliTPCcluster*)GetCluster(idx);
      Double_t chi2 = seed->GetPredictedChi2(c);
      
      index=idx; 
      cl=c; 
      maxchi2=chi2;
      
      if (++nc < track->GetNumberOfClusters() ) {
        idx=track->GetClusterIndex(nc); 
        sec=(idx&0xff000000)>>24; 
        row=(idx&0x00ff0000)>>16;
      }
            
      if (fSectors==fInnerSec) { if (sec >= fkNIS) row=-1; }
      else { if (sec < fkNIS) row=-1; }   
      
    }
    
    if (cl) {
      
      //cout << "Assigned" << endl;
      Float_t l=fSectors->GetPadPitchWidth();
      Float_t corr=1.; if (i>63) corr=0.67; // new (third) pad response !
      seed->SetSampledEdx(cl->GetQ()/l*corr,seed->GetNumberOfClusters());
      seed->Update(cl,maxchi2,index);
    }
  }

  seed->SetLabel(nc);
  return 1;
}

//_____________________________________________________________________________
Int_t AliTPCtracker::FollowBackProlongation
(AliTPCseed& seed, const AliTPCtrack &track) {
  //-----------------------------------------------------------------
  // This function propagates tracks back through the TPC
  //-----------------------------------------------------------------
  Double_t alpha=seed.GetAlpha() - fSectors->GetAlphaShift();
  if (alpha > 2.*TMath::Pi()) alpha -= 2.*TMath::Pi();  
  if (alpha < 0.            ) alpha += 2.*TMath::Pi();  
  Int_t s=Int_t(alpha/fSectors->GetAlpha())%fN;

  Int_t idx=-1, sec=-1, row=-1;
  Int_t nc=seed.GetLabel(); //index of the cluster to start with
  if (nc--) {
     idx=track.GetClusterIndex(nc);
     sec=(idx&0xff000000)>>24; row=(idx&0x00ff0000)>>16;
  }
  if (fSectors==fInnerSec) { if (sec >= fkNIS) row=-1; } 
  else { if (sec <  fkNIS) row=-1; }   

  Int_t nr=fSectors->GetNRows();
  for (Int_t i=0; i<nr; i++) {
    Double_t x=fSectors->GetX(i), ymax=fSectors->GetMaxY(i);

    if (!seed.PropagateTo(x)) return 0;

    Double_t y=seed.GetY();
    if (y > ymax) {
       s = (s+1) % fN;
       if (!seed.Rotate(fSectors->GetAlpha())) return 0;
    } else if (y <-ymax) {
       s = (s-1+fN) % fN;
       if (!seed.Rotate(-fSectors->GetAlpha())) return 0;
    }

    AliTPCcluster *cl=0;
    Int_t index=0;
    Double_t maxchi2=kMaxCHI2;
    Double_t pt=seed.GetConvConst()/(100/0.299792458/0.2)/seed.Get1Pt();
    Double_t sy2=SigmaY2(seed.GetX(),seed.GetTgl(),pt);
    Double_t sz2=SigmaZ2(seed.GetX(),seed.GetTgl());
    Double_t road=4.*sqrt(seed.GetSigmaY2() + sy2), z=seed.GetZ();
    if (road>kMaxROAD) {
      cerr<<seed.GetNumberOfClusters()
          <<"AliTPCtracker::FollowBackProlongation: Too broad road !\n"; 
      return 0;
    }


    Int_t accepted=seed.GetNumberOfClusters();
    if (row==i) {
       //try to accept already found cluster
       AliTPCcluster *c=(AliTPCcluster*)GetCluster(idx);
       Double_t chi2;
       if ((chi2=seed.GetPredictedChi2(c))<maxchi2 || accepted<27) { 
          index=idx; cl=c; maxchi2=chi2;
       } //else cerr<<"AliTPCtracker::FollowBackProlongation: oulier !\n";
          
       if (nc--) {
          idx=track.GetClusterIndex(nc); 
          sec=(idx&0xff000000)>>24; row=(idx&0x00ff0000)>>16;
       } 
       if (fSectors==fInnerSec) { if (sec >= fkNIS) row=-1; }
       else { if (sec < fkNIS) row=-1; }   

    }
    if (!cl) {
       //try to fill the gap
       const AliTPCRow &krow=fSectors[s][i];
       if (accepted>27)
       if (krow) {
          for (Int_t i=krow.Find(y-road); i<krow; i++) {
	    AliTPCcluster *c=(AliTPCcluster*)(krow[i]);
	    if (c->GetY() > y+road) break;
	    if (c->IsUsed()) continue;
	 if ((c->GetZ()-z)*(c->GetZ()-z)>16.*(seed.GetSigmaZ2()+sz2)) continue;
	    Double_t chi2=seed.GetPredictedChi2(c);
	    if (chi2 > maxchi2) continue;
	    maxchi2=chi2;
	    cl=c;
            index=krow.GetIndex(i);
          }
       }
    }

    if (cl) {
      Float_t l=fSectors->GetPadPitchWidth();
      Float_t corr=1.; if (i>63) corr=0.67; // new (third) pad response !
      seed.SetSampledEdx(cl->GetQ()/l*corr,seed.GetNumberOfClusters());
      seed.Update(cl,maxchi2,index);
    }

  }

  seed.SetLabel(nc);

  return 1;
}

//_____________________________________________________________________________
void AliTPCtracker::MakeSeeds(Int_t i1, Int_t i2) {
  //-----------------------------------------------------------------
  // This function creates track seeds.
  //-----------------------------------------------------------------
  Double_t x[5], c[15];

  Double_t alpha=fSectors->GetAlpha(), shift=fSectors->GetAlphaShift();
  Double_t cs=cos(alpha), sn=sin(alpha);

  Double_t x1 =fSectors->GetX(i1);
  Double_t xx2=fSectors->GetX(i2);

  for (Int_t ns=0; ns<fN; ns++) {
    Int_t nl=fSectors[(ns-1+fN)%fN][i2];
    Int_t nm=fSectors[ns][i2];
    Int_t nu=fSectors[(ns+1)%fN][i2];
    const AliTPCRow& kr1=fSectors[ns][i1];
    for (Int_t is=0; is < kr1; is++) {
      Double_t y1=kr1[is]->GetY(), z1=kr1[is]->GetZ();
      for (Int_t js=0; js < nl+nm+nu; js++) {
	const AliTPCcluster *kcl;
        Double_t x2,   y2,   z2;
        Double_t x3=GetX(), y3=GetY(), z3=GetZ();

	if (js<nl) {
	  const AliTPCRow& kr2=fSectors[(ns-1+fN)%fN][i2];
	  kcl=kr2[js];
          y2=kcl->GetY(); z2=kcl->GetZ();
          x2= xx2*cs+y2*sn;
          y2=-xx2*sn+y2*cs;
	} else 
	  if (js<nl+nm) {
	    const AliTPCRow& kr2=fSectors[ns][i2];
	    kcl=kr2[js-nl];
            x2=xx2; y2=kcl->GetY(); z2=kcl->GetZ();
	  } else {
	    const AliTPCRow& kr2=fSectors[(ns+1)%fN][i2];
	    kcl=kr2[js-nl-nm];
            y2=kcl->GetY(); z2=kcl->GetZ();
            x2=xx2*cs-y2*sn;
            y2=xx2*sn+y2*cs;
	  }

        Double_t zz=z1 - (z1-z3)/(x1-x3)*(x1-x2); 
        if (TMath::Abs(zz-z2)>5.) continue;

        Double_t d=(x2-x1)*(0.-y2)-(0.-x2)*(y2-y1);
        if (d==0.) {cerr<<"MakeSeeds warning: Straight seed !\n"; continue;}

	x[0]=y1;
	x[1]=z1;
	x[4]=f1(x1,y1,x2,y2,x3,y3);
	if (TMath::Abs(x[4]) >= 0.0066) continue;
	x[2]=f2(x1,y1,x2,y2,x3,y3);
	//if (TMath::Abs(x[4]*x1-x[2]) >= 0.99999) continue;
	x[3]=f3(x1,y1,x2,y2,z1,z2);
	if (TMath::Abs(x[3]) > 1.2) continue;
	Double_t a=asin(x[2]);
	Double_t zv=z1 - x[3]/x[4]*(a+asin(x[4]*x1-x[2]));
	if (TMath::Abs(zv-z3)>10.) continue; 

        Double_t sy1=kr1[is]->GetSigmaY2(), sz1=kr1[is]->GetSigmaZ2();
        Double_t sy2=kcl->GetSigmaY2(),     sz2=kcl->GetSigmaZ2();
	//Double_t sy3=400*3./12., sy=0.1, sz=0.1;
        Double_t sy3=25000*x[4]*x[4]+0.1, sy=0.1, sz=0.1;

	Double_t f40=(f1(x1,y1+sy,x2,y2,x3,y3)-x[4])/sy;
	Double_t f42=(f1(x1,y1,x2,y2+sy,x3,y3)-x[4])/sy;
	Double_t f43=(f1(x1,y1,x2,y2,x3,y3+sy)-x[4])/sy;
	Double_t f20=(f2(x1,y1+sy,x2,y2,x3,y3)-x[2])/sy;
	Double_t f22=(f2(x1,y1,x2,y2+sy,x3,y3)-x[2])/sy;
	Double_t f23=(f2(x1,y1,x2,y2,x3,y3+sy)-x[2])/sy;
	Double_t f30=(f3(x1,y1+sy,x2,y2,z1,z2)-x[3])/sy;
	Double_t f31=(f3(x1,y1,x2,y2,z1+sz,z2)-x[3])/sz;
	Double_t f32=(f3(x1,y1,x2,y2+sy,z1,z2)-x[3])/sy;
	Double_t f34=(f3(x1,y1,x2,y2,z1,z2+sz)-x[3])/sz;

        c[0]=sy1;
        c[1]=0.;       c[2]=sz1;
        c[3]=f20*sy1;  c[4]=0.;       c[5]=f20*sy1*f20+f22*sy2*f22+f23*sy3*f23;
        c[6]=f30*sy1;  c[7]=f31*sz1;  c[8]=f30*sy1*f20+f32*sy2*f22;
                       c[9]=f30*sy1*f30+f31*sz1*f31+f32*sy2*f32+f34*sz2*f34;
        c[10]=f40*sy1; c[11]=0.; c[12]=f40*sy1*f20+f42*sy2*f22+f43*sy3*f23;
        c[13]=f30*sy1*f40+f32*sy2*f42;
        c[14]=f40*sy1*f40+f42*sy2*f42+f43*sy3*f43;

        UInt_t index=kr1.GetIndex(is);
	AliTPCseed *track=new AliTPCseed(index, x, c, x1, ns*alpha+shift);
        Float_t l=fSectors->GetPadPitchWidth();
        track->SetSampledEdx(kr1[is]->GetQ()/l,0);

        Int_t rc=FollowProlongation(*track, i2);
        if (rc==0 || track->GetNumberOfClusters()<(i1-i2)/2) delete track;
        else fSeeds->AddLast(track); 
      }
    }
  }
}

//_____________________________________________________________________________
Int_t AliTPCtracker::ReadSeeds(const TFile *inp) {
  //-----------------------------------------------------------------
  // This function reades track seeds.
  //-----------------------------------------------------------------
  TDirectory *savedir=gDirectory; 

  TFile *in=(TFile*)inp;
  if (!in->IsOpen()) {
     cerr<<"AliTPCtracker::ReadSeeds(): input file is not open !\n";
     return 1;
  }

  in->cd();
  TTree *seedTree=(TTree*)in->Get("Seeds");
  if (!seedTree) {
     cerr<<"AliTPCtracker::ReadSeeds(): ";
     cerr<<"can't get a tree with track seeds !\n";
     return 2;
  }
  AliTPCtrack *seed=new AliTPCtrack; 
  seedTree->SetBranchAddress("tracks",&seed);
  
  if (fSeeds==0) fSeeds=new TObjArray(15000);

  Int_t n=(Int_t)seedTree->GetEntries();
  for (Int_t i=0; i<n; i++) {
     seedTree->GetEvent(i);
     fSeeds->AddLast(new AliTPCseed(*seed,seed->GetAlpha()));
  }
  
  delete seed;

  delete seedTree; //Thanks to Mariana Bondila

  savedir->cd();

  return 0;
}

//_____________________________________________________________________________
Int_t AliTPCtracker::Clusters2Tracks(const TFile *inp, TFile *out) {
  //-----------------------------------------------------------------
  // This is a track finder.
  //-----------------------------------------------------------------
  TDirectory *savedir=gDirectory; 

  if (inp) {
     TFile *in=(TFile*)inp;
     if (!in->IsOpen()) {
        cerr<<"AliTPCtracker::Clusters2Tracks(): input file is not open !\n";
        return 1;
     }
  }

  if (!out->IsOpen()) {
     cerr<<"AliTPCtracker::Clusters2Tracks(): output file is not open !\n";
     return 2;
  }

  LoadClusters();

  out->cd();

  char   tname[100];
  sprintf(tname,"TreeT_TPC_%d",GetEventNumber());
  TTree tracktree(tname,"Tree with TPC tracks");
  AliTPCtrack *iotrack=0;
  tracktree.Branch("tracks","AliTPCtrack",&iotrack,32000,3);

  //find track seeds
  Int_t nup=fOuterSec->GetNRows(), nlow=fInnerSec->GetNRows();
  Int_t nrows=nlow+nup;
  if (fSeeds==0) {
     Int_t gap=Int_t(0.125*nrows), shift=Int_t(0.5*gap);
     fSectors=fOuterSec; fN=fkNOS;
     fSeeds=new TObjArray(15000);     
     MakeSeeds(nup-1, nup-1-gap);
     MakeSeeds(nup-1-shift, nup-1-shift-gap);
  }
  fSeeds->Sort();

  Int_t found=0;
  Int_t nseed=fSeeds->GetEntriesFast();
  for (Int_t i=0; i<nseed; i++) {
    //tracking in the outer sectors
    fSectors=fOuterSec; fN=fkNOS;

    AliTPCseed *pt=(AliTPCseed*)fSeeds->UncheckedAt(i), &t=*pt;
    if (!FollowProlongation(t)) {
       delete fSeeds->RemoveAt(i);
       continue;
    }

    //tracking in the inner sectors
    fSectors=fInnerSec; fN=fkNIS;

    Double_t alpha=t.GetAlpha() - fInnerSec->GetAlphaShift();
    if (alpha > 2.*TMath::Pi()) alpha -= 2.*TMath::Pi();
    if (alpha < 0.            ) alpha += 2.*TMath::Pi();
    Int_t ns=Int_t(alpha/fInnerSec->GetAlpha())%fkNIS;

    alpha=ns*fInnerSec->GetAlpha()+fInnerSec->GetAlphaShift()-t.GetAlpha();

    if (t.Rotate(alpha)) {
      if (FollowProlongation(t)) {
        if (t.GetNumberOfClusters() >= Int_t(0.4*nrows)) {
          t.CookdEdx();
          CookLabel(pt,0.1); //For comparison only
          iotrack=pt;
          tracktree.Fill();
          UseClusters(&t);
          cerr<<found++<<'\r';
        }
      }
    }
    delete fSeeds->RemoveAt(i); 
  }
  
  tracktree.Write();

  cerr<<"Number of found tracks : "<<found<<endl;

  savedir->cd();

  UnloadClusters();
  fSeeds->Clear(); delete fSeeds; fSeeds=0;

  return 0;
}
//_____________________________________________________________________________

Int_t AliTPCtracker::RefitInward(TFile *in, TFile *out) {
  //
  // The function propagates tracks throught TPC inward
  // using already associated clusters.
  //
  // in - file with back propagated tracks
  // outs  - file with inward propagation
  //
  // Sylwester Radomski, GSI
  // 26.02.2003
  //


  if (!in->IsOpen()) {
    cout << "Input File not open !\n" << endl;
    return 1;
  }
  
  if (!out->IsOpen()) {
    cout << "Output File not open !\n" << endl;
    return 2;
  }

  TDirectory *savedir = gDirectory; 
  LoadClusters();

  // Connect to input tree
  in->cd();
  TTree *inputTree = (TTree*)in->Get("seedsTPCtoTRD_0");
  TBranch *inBranch = inputTree->GetBranch("tracks");
  AliTPCtrack *inTrack = 0;
  inBranch->SetAddress(&inTrack);

  Int_t nTracks = (Int_t)inputTree->GetEntries();

  // Create output tree
  out->cd(); 
  AliTPCtrack *outTrack = new AliTPCtrack();
  TTree *outputTree = new TTree("tracksTPC_0","Refited TPC tracks");
  outputTree->Branch("tracks", "AliTPCtrack", &outTrack, 32000, 3);

  Int_t nRefited = 0;

  for(Int_t t=0; t < nTracks; t++) {
    
    cout << t << "\r";

    inputTree->GetEvent(t);
    AliTPCseed *seed = new AliTPCseed(*inTrack, inTrack->GetAlpha());

    seed->ResetCovariance();

    seed->SetLabel(0);    
    fSectors = fInnerSec;
    Int_t res = FollowRefitInward(seed, inTrack);
    UseClusters(seed);
    Int_t nc = seed->GetNumberOfClusters();

    fSectors = fOuterSec;
    res = FollowRefitInward(seed, inTrack);
    UseClusters(seed, nc);

    if (res) {
      seed->PropagateTo(fParam->GetInnerRadiusLow());
      outTrack = (AliTPCtrack*)seed;
      outTrack->SetLabel(inTrack->GetLabel());
      outputTree->Fill();
      nRefited++;
    }

    delete seed;
  }

  cout << "Refitted " << nRefited << " tracks." << endl;

  outputTree->Write();
  
  if (inputTree) delete inputTree;
  if (outputTree) delete outputTree;

  savedir->cd();
  UnloadClusters();

  return 0;
}

//_____________________________________________________________________________
Int_t AliTPCtracker::PropagateBack(const TFile *inp, TFile *out) {
  //-----------------------------------------------------------------
  // This function propagates tracks back through the TPC.
  //-----------------------------------------------------------------
  fSeeds=new TObjArray(15000);

  TFile *in=(TFile*)inp;
  TDirectory *savedir=gDirectory; 

  if (!in->IsOpen()) {
     cerr<<"AliTPCtracker::PropagateBack(): ";
     cerr<<"file with back propagated ITS tracks is not open !\n";
     return 1;
  }

  if (!out->IsOpen()) {
     cerr<<"AliTPCtracker::PropagateBack(): ";
     cerr<<"file for back propagated TPC tracks is not open !\n";
     return 2;
  }

  LoadClusters();

  in->cd();
  char tName[100];
  sprintf(tName,"TreeT_ITSb_%d",GetEventNumber());
  TTree *bckTree=(TTree*)in->Get(tName);
  if (!bckTree) {
     cerr<<"AliTPCtracker::PropagateBack() ";
     cerr<<"can't get a tree with back propagated ITS tracks !\n";
     return 3;
  }
  AliTPCtrack *bckTrack=new AliTPCtrack; 
  bckTree->SetBranchAddress("tracks",&bckTrack);

  sprintf(tName,"TreeT_TPC_%d",GetEventNumber());
  TTree *tpcTree=(TTree*)in->Get(tName);
  if (!tpcTree) {
     cerr<<"AliTPCtracker::PropagateBack() ";
     cerr<<"can't get a tree with TPC tracks !\n";
     return 4;
  }
  AliTPCtrack *tpcTrack=new AliTPCtrack; 
  tpcTree->SetBranchAddress("tracks",&tpcTrack);

  //*** Prepare an array of tracks to be back propagated
  Int_t nup=fOuterSec->GetNRows(), nlow=fInnerSec->GetNRows();
  Int_t nrows=nlow+nup;

  //
  // Match ITS tracks with old TPC tracks
  // the tracks do not have to be sorted [SR, GSI, 18.02.2003]
  //
  // the algorithm is linear and uses LUT for sorting
  // cost of the algorithm: 2 * number of tracks
  //

  TObjArray tracks(15000);
  Int_t i=0;
  Int_t tpcN= (Int_t)tpcTree->GetEntries();
  Int_t bckN= (bckTree)? (Int_t)bckTree->GetEntries() : 0;

  // look up table   
  const Int_t nLab = 20000; // limit on number of primaries (arbitrary)
  Int_t lut[nLab];
  for(Int_t i=0; i<nLab; i++) lut[i] = -1;
    
  if (bckTree) {
    for(Int_t i=0; i<bckN; i++) {
      bckTree->GetEvent(i);
      Int_t lab = TMath::Abs(bckTrack->GetLabel());
      if (lab < nLab) lut[lab] = i;
    }
  }
  
  for (Int_t i=0; i<tpcN; i++) {
    tpcTree->GetEvent(i);
    Double_t alpha=tpcTrack->GetAlpha();
    
    if (!bckTree) { 

      // No ITS - use TPC track only
      
      fSeeds->AddLast(new AliTPCseed(*tpcTrack, tpcTrack->GetAlpha()));
      tracks.AddLast(new AliTPCtrack(*tpcTrack));
    
    } else {  
      
      // with ITS
      // discard not prolongated tracks (to be discussed)

      Int_t lab = TMath::Abs(tpcTrack->GetLabel());
      if (lab > nLab || lut[lab] < 0) continue;

      bckTree->GetEvent(lut[lab]);   
      bckTrack->Rotate(alpha-bckTrack->GetAlpha());

      fSeeds->AddLast(new AliTPCseed(*bckTrack,bckTrack->GetAlpha()));
      tracks.AddLast(new AliTPCtrack(*tpcTrack));
    }
  }

  // end of matching

  /*
  TObjArray tracks(15000);
  Int_t i=0, j=0;
  Int_t tpcN=(Int_t)tpcTree->GetEntries();
  Int_t bckN=(Int_t)bckTree->GetEntries();
  if (j<bckN) bckTree->GetEvent(j++);
  for (i=0; i<tpcN; i++) {
     tpcTree->GetEvent(i);
     Double_t alpha=tpcTrack->GetAlpha();
     if (j<bckN &&
     TMath::Abs(tpcTrack->GetLabel())==TMath::Abs(bckTrack->GetLabel())) {
        if (!bckTrack->Rotate(alpha-bckTrack->GetAlpha())) continue;
        fSeeds->AddLast(new AliTPCseed(*bckTrack,bckTrack->GetAlpha()));
        bckTree->GetEvent(j++);
     } else {
        tpcTrack->ResetCovariance();
        fSeeds->AddLast(new AliTPCseed(*tpcTrack,alpha));
     }
     tracks.AddLast(new AliTPCtrack(*tpcTrack));
  }
  */
  
  out->cd();

  // tree name seedsTPCtoTRD as expected by TRD and as 
  // discussed and decided in Strasbourg (May 2002)
  // [SR, GSI, 18.02.2003]
  
  sprintf(tName,"seedsTPCtoTRD_%d",GetEventNumber());
  TTree backTree(tName,"Tree with back propagated TPC tracks");

  AliTPCtrack *otrack=0;
  backTree.Branch("tracks","AliTPCtrack",&otrack,32000,0);

  //*** Back propagation through inner sectors
  fSectors=fInnerSec; fN=fkNIS;
  Int_t nseed=fSeeds->GetEntriesFast();
  for (i=0; i<nseed; i++) {
     AliTPCseed *ps=(AliTPCseed*)fSeeds->UncheckedAt(i), &s=*ps;
     const AliTPCtrack *pt=(AliTPCtrack*)tracks.UncheckedAt(i), &t=*pt;

     Int_t nc=t.GetNumberOfClusters();
     s.SetLabel(nc-1); //set number of the cluster to start with

     if (FollowBackProlongation(s,t)) {
	UseClusters(&s);
        continue;
     }
     delete fSeeds->RemoveAt(i);
  }  

//*** Back propagation through outer sectors
  fSectors=fOuterSec; fN=fkNOS;
  Int_t found=0;
  for (i=0; i<nseed; i++) {
     AliTPCseed *ps=(AliTPCseed*)fSeeds->UncheckedAt(i), &s=*ps;
     if (!ps) continue;
     Int_t nc=s.GetNumberOfClusters();
     const AliTPCtrack *pt=(AliTPCtrack*)tracks.UncheckedAt(i), &t=*pt;

     Double_t alpha=s.GetAlpha() - fSectors->GetAlphaShift();
     if (alpha > 2.*TMath::Pi()) alpha -= 2.*TMath::Pi();
     if (alpha < 0.            ) alpha += 2.*TMath::Pi();
     Int_t ns=Int_t(alpha/fSectors->GetAlpha())%fN;

     alpha =ns*fSectors->GetAlpha() + fSectors->GetAlphaShift();
     alpha-=s.GetAlpha();

     if (s.Rotate(alpha)) {
       if (FollowBackProlongation(s,t)) {
         if (s.GetNumberOfClusters() >= Int_t(0.4*nrows)) {
           s.CookdEdx();
           s.SetLabel(t.GetLabel());
           UseClusters(&s,nc);
           
           // Propagate to outer reference plane for comparison reasons
           // reason for keeping fParam object [SR, GSI, 18.02.2003] 
         
           ps->PropagateTo(fParam->GetOuterRadiusUp()); 
           otrack=ps;
           backTree.Fill();
           cerr<<found++<<'\r';
           continue;
         }
       }
     }
     delete fSeeds->RemoveAt(i);
  }  

  backTree.Write();
  savedir->cd();
  cerr<<"Number of seeds: "<<nseed<<endl;
  cerr<<"Number of back propagated ITS tracks: "<<bckN<<endl;
  cerr<<"Number of back propagated TPC tracks: "<<found<<endl;

  delete bckTrack;
  delete tpcTrack;

  delete bckTree; //Thanks to Mariana Bondila
  delete tpcTree; //Thanks to Mariana Bondila

  UnloadClusters();  

  return 0;
}

//_________________________________________________________________________
AliCluster *AliTPCtracker::GetCluster(Int_t index) const {
  //--------------------------------------------------------------------
  //       Return pointer to a given cluster
  //--------------------------------------------------------------------
  Int_t sec=(index&0xff000000)>>24; 
  Int_t row=(index&0x00ff0000)>>16; 
  Int_t ncl=(index&0x0000ffff)>>00;

  const AliTPCcluster *cl=0;

  if (sec<fkNIS) {
    cl=fInnerSec[sec][row].GetUnsortedCluster(ncl); 
  } else {
    sec-=fkNIS;
    cl=fOuterSec[sec][row].GetUnsortedCluster(ncl);
  }

  return (AliCluster*)cl;      
}

//__________________________________________________________________________
void AliTPCtracker::CookLabel(AliKalmanTrack *t, Float_t wrong) const {
  //--------------------------------------------------------------------
  //This function "cooks" a track label. If label<0, this track is fake.
  //--------------------------------------------------------------------
  Int_t noc=t->GetNumberOfClusters();
  Int_t *lb=new Int_t[noc];
  Int_t *mx=new Int_t[noc];
  AliCluster **clusters=new AliCluster*[noc];

  Int_t i;
  for (i=0; i<noc; i++) {
     lb[i]=mx[i]=0;
     Int_t index=t->GetClusterIndex(i);
     clusters[i]=GetCluster(index);
  }

  Int_t lab=123456789;
  for (i=0; i<noc; i++) {
    AliCluster *c=clusters[i];
    lab=TMath::Abs(c->GetLabel(0));
    Int_t j;
    for (j=0; j<noc; j++) if (lb[j]==lab || mx[j]==0) break;
    lb[j]=lab;
    (mx[j])++;
  }

  Int_t max=0;
  for (i=0; i<noc; i++) if (mx[i]>max) {max=mx[i]; lab=lb[i];}
    
  for (i=0; i<noc; i++) {
    AliCluster *c=clusters[i];
    if (TMath::Abs(c->GetLabel(1)) == lab ||
        TMath::Abs(c->GetLabel(2)) == lab ) max++;
  }

  if ((1.- Float_t(max)/noc) > wrong) lab=-lab;

  else {
     Int_t tail=Int_t(0.10*noc);
     max=0;
     for (i=1; i<=tail; i++) {
       AliCluster *c=clusters[noc-i];
       if (lab == TMath::Abs(c->GetLabel(0)) ||
           lab == TMath::Abs(c->GetLabel(1)) ||
           lab == TMath::Abs(c->GetLabel(2))) max++;
     }
     if (max < Int_t(0.5*tail)) lab=-lab;
  }

  t->SetLabel(lab);

  delete[] lb;
  delete[] mx;
  delete[] clusters;
}

//_________________________________________________________________________
void AliTPCtracker::AliTPCSector::Setup(const AliTPCParam *par, Int_t f) {
  //-----------------------------------------------------------------------
  // Setup inner sector
  //-----------------------------------------------------------------------
  if (f==0) {
     fAlpha=par->GetInnerAngle();
     fAlphaShift=par->GetInnerAngleShift();
     fPadPitchWidth=par->GetInnerPadPitchWidth();
     f1PadPitchLength=par->GetInnerPadPitchLength();
     f2PadPitchLength=f1PadPitchLength;

     fN=par->GetNRowLow();
     fRow=new AliTPCRow[fN];
     for (Int_t i=0; i<fN; i++) fRow[i].SetX(par->GetPadRowRadiiLow(i));
  } else {
     fAlpha=par->GetOuterAngle();
     fAlphaShift=par->GetOuterAngleShift();
     fPadPitchWidth=par->GetOuterPadPitchWidth();
     f1PadPitchLength=par->GetOuter1PadPitchLength();
     f2PadPitchLength=par->GetOuter2PadPitchLength();

     fN=par->GetNRowUp();
     fRow=new AliTPCRow[fN];
     for (Int_t i=0; i<fN; i++){ 
     fRow[i].SetX(par->GetPadRowRadiiUp(i));
     }
  } 
}

//_________________________________________________________________________
void AliTPCtracker::
AliTPCRow::InsertCluster(const AliTPCcluster* c, Int_t sec, Int_t row) {
  //-----------------------------------------------------------------------
  // Insert a cluster into this pad row in accordence with its y-coordinate
  //-----------------------------------------------------------------------
  if (fN==kMaxClusterPerRow) {
    cerr<<"AliTPCRow::InsertCluster(): Too many clusters !\n"; return;
  }

  Int_t index=(((sec<<8)+row)<<16)+fN;

  if (fN==0) {
     fIndex[0]=index;
     fClusterArray[0]=*c; fClusters[fN++]=fClusterArray; 
     return;
  }

  if (fN==fSize) {
     Int_t size=fSize*2;
     AliTPCcluster *buff=new AliTPCcluster[size];
     memcpy(buff,fClusterArray,fSize*sizeof(AliTPCcluster));
     for (Int_t i=0; i<fN; i++) 
        fClusters[i]=buff+(fClusters[i]-fClusterArray);
     delete[] fClusterArray;
     fClusterArray=buff;
     fSize=size;
  }

  Int_t i=Find(c->GetY());
  memmove(fClusters+i+1 ,fClusters+i,(fN-i)*sizeof(AliTPCcluster*));
  memmove(fIndex   +i+1 ,fIndex   +i,(fN-i)*sizeof(UInt_t));
  fIndex[i]=index; 
  fClusters[i]=fClusterArray+fN; fClusterArray[fN++]=*c;
}

//___________________________________________________________________
Int_t AliTPCtracker::AliTPCRow::Find(Double_t y) const {
  //-----------------------------------------------------------------------
  // Return the index of the nearest cluster 
  //-----------------------------------------------------------------------
  if(fN<=0) return 0;
  if (y <= fClusters[0]->GetY()) return 0;
  if (y > fClusters[fN-1]->GetY()) return fN;
  Int_t b=0, e=fN-1, m=(b+e)/2;
  for (; b<e; m=(b+e)/2) {
    if (y > fClusters[m]->GetY()) b=m+1;
    else e=m; 
  }
  return m;
}

//_____________________________________________________________________________
void AliTPCtracker::AliTPCseed::CookdEdx(Double_t low, Double_t up) {
  //-----------------------------------------------------------------
  // This funtion calculates dE/dX within the "low" and "up" cuts.
  //-----------------------------------------------------------------
  Int_t i;
  Int_t nc=GetNumberOfClusters();

  Int_t swap;//stupid sorting
  do {
    swap=0;
    for (i=0; i<nc-1; i++) {
      if (fdEdxSample[i]<=fdEdxSample[i+1]) continue;
      Float_t tmp=fdEdxSample[i];
      fdEdxSample[i]=fdEdxSample[i+1]; fdEdxSample[i+1]=tmp;
      swap++;
    }
  } while (swap);

  Int_t nl=Int_t(low*nc), nu=Int_t(up*nc);
  Float_t dedx=0;
  for (i=nl; i<=nu; i++) dedx += fdEdxSample[i];
  dedx /= (nu-nl+1);
  SetdEdx(dedx);

  //Very rough PID
  Double_t p=TMath::Sqrt((1.+ GetTgl()*GetTgl())/(Get1Pt()*Get1Pt()));

  Double_t log1=TMath::Log(p+0.45), log2=TMath::Log(p+0.12);
  if (p<0.6) {
    if (dedx < 34 + 30/(p+0.45)/(p+0.45) + 24*log1) {SetMass(0.13957); return;}
    if (dedx < 34 + 30/(p+0.12)/(p+0.12) + 24*log2) {SetMass(0.49368); return;}
    SetMass(0.93827); return;
  }

  if (p<1.2) {
    if (dedx < 34 + 30/(p+0.12)/(p+0.12) + 24*log2) {SetMass(0.13957); return;}
    SetMass(0.93827); return;
  }

  SetMass(0.13957); return;

}




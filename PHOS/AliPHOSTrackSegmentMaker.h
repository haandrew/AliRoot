#ifndef ALIPHOSTRACKSEGMENTMAKER_H
#define ALIPHOSTRACKSEGMENTMAKER_H
/* Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 * See cxx source for full Copyright notice                               */

/* $Id$ */

//_________________________________________________________________________
// Algorithm Base class to construct PHOS track segments
// Associates EMC and PPSD clusters
// Unfolds the EMC cluster   
//                  
//*-- Author: Dmitri Peressounko (RRC Kurchatov Institute  & SUBATECH)

// --- ROOT system ---
#include "TTask.h"
#include "AliConfig.h"
class TFile ; 

// --- Standard library ---
#include <iostream>

// --- AliRoot header files ---


class AliPHOSClusterizer ;
class AliPHOSGeometry ;

class  AliPHOSTrackSegmentMaker : public TTask {

public:

  AliPHOSTrackSegmentMaker();
  AliPHOSTrackSegmentMaker(const TString alirunFileName, const TString eventFolderName = AliConfig::fgkDefaultEventFolderName) ;                     
  AliPHOSTrackSegmentMaker(const AliPHOSTrackSegmentMaker & tsmaker) { ; } 
  virtual ~ AliPHOSTrackSegmentMaker() ;

  virtual void    Exec(Option_t * option){Warning("Exec", "Not Defined" ) ; } 

  virtual const Int_t GetTrackSegmentsInRun()  const {Warning("GetTrackSegmentsInRun", "Not Defined" ) ; return 0 ; } 

  virtual void    Print()const {Warning("Print", "Not Defined" ) ; }
  void   SetEventFolderName(TString name) { fEventFolderName = name ; }

  virtual void WriteTrackSegments(Int_t event){Warning("WriteTrackSegments", "Not Defined" ) ; } 
  
protected:
  TString fEventFolderName ;  // event folder name

  ClassDef( AliPHOSTrackSegmentMaker,3)    // Algorithm class to make PHOS track segments (Base Class)
};

#endif // ALIPHOSTRACKSEGMENTMAKER_H

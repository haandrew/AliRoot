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


//_________________________________________________________________________
// Class to calculate calibration parameters  from beam tests etc.
//  First pass - one should calcuate pedestals, in the second pass - gains.
//
//     To calculate pedestals we scan pedestals events and fill histos for each
//     channel. Then in each histo we find maximum and fit it with Gaussian in the 
//     visinity of the maximum. Extracted mean and width of distribution are put into 
//     resulting histogram which cheks results for 'reasonability'.
//
//     To evaluate gains: scans beam events and calculate gain in the cristall with 
//     maximal energy deposition, assuming, that 90% of energy deposited in it. 
//     For each channel separate histogramm is filled. When scan is finished, 
//     these histograms are fitted with Gaussian and finds mean and width, which 
//     are put in final histogram. Finaly gains are checked for deviation from mean. 

//     Finally fills database.
//
// Use Case:
//       AliPHOSCalibrator * c = new AliPHOSCalibrator("path/galice.root") ;
//       c->AddRun("path2/galice.root") ; 
//       c->ScanPedestals();
//       c->CalculatePedestals();             
//       c->WritePedestals();
//       c->ScanGains() ;
//       c->CalculateGains() ;
//       c->WriteGains() ;
//
//*-- Author : D.Peressounko  (RRC KI) 
//////////////////////////////////////////////////////////////////////////////

// --- ROOT system ---
#include "TROOT.h"
#include "TF1.h"
#include "TObjString.h"
// --- Standard library ---

// --- AliRoot header files ---
#include "AliPHOSGetter.h"
#include "AliPHOSCalibrator.h"
#include "AliPHOSConTableDB.h"
#include "AliPHOSCalibrManager.h"
#include "AliPHOSCalibrationData.h"

ClassImp(AliPHOSCalibrator)


//____________________________________________________________________________ 
  AliPHOSCalibrator::AliPHOSCalibrator():TTask("AliPHOSCalibrator","Default") 
{
  //Default constuctor for root. Normally should not be used
  fRunList=0 ;
  fNch = 0 ;
  fPedHistos = 0 ;
  fGainHistos = 0 ;
  fhPedestals = 0 ;
  fhPedestalsWid = 0 ;
  fctdb = 0 ;
  fConTableDB = "Beamtest2002" ;
  fConTableDBFile = "ConTableDB.root" ;
}
//____________________________________________________________________________ 
AliPHOSCalibrator::AliPHOSCalibrator(const char* file, const char* title):
  TTask("AliPHOSCalibrator",title) 
{ 
  //Constructor which should normally be used.
  //file: path/galice.root  - header file
  //title: branch name of PHOS reconstruction (e.g. "Default")
 

  fRunList = new TList() ;
  fRunList->SetOwner() ;
  fRunList->Add(new TObjString(file)) ;
  fNch = 0 ;
  fPedPat = 257 ;  //Patterns for different kind of events
  fPulPat = 33 ;
  fLEDPat = 129 ;
  fWBPat  = 1027 ;
  fNBPat  = 1029 ;

  fNChan  = 100 ;  
  fGainMax = 0.1 ;
  fNGainBins= 100 ;
  fAcceptCorr = 10 ;     //Maximal deviation from mean, considered as normal 

  fGainAcceptCorr = 5 ;  //Factor for gain deviation
  fPedHistos = 0 ;
  fGainHistos = 0 ;
  fhPedestals = 0 ;
  fhPedestalsWid = 0 ;
  fctdb = 0 ;
  fConTableDB = "Beamtest2002" ;
  fConTableDBFile = "ConTableDB.root" ;
}

//____________________________________________________________________________ 
  AliPHOSCalibrator::~AliPHOSCalibrator()
{
  // dtor
  if(fPedHistos)
    delete fPedHistos ;
  if(fGainHistos)
    delete fGainHistos ;
  if(fhPedestals)
    delete fhPedestals ;
  if(fhPedestalsWid)
    delete fhPedestalsWid ;
  if(fctdb)
    delete fctdb ;
  if(fRunList)
    delete fRunList ;
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::AddRun(const char * filename)
{
  //Adds one more run to list of runs, which will be scanned in ScanXXX methods
  
  TObjString * fn = new TObjString(filename) ;
  if(!fRunList){
    fRunList=new TList() ;
    fRunList->SetOwner() ;
    fRunList->Add(fn) ;
    return ;
  }
  else{
    TIter next(fRunList) ;
    TObjString * r ;
    while((r=(TObjString *)(next()))){
      if(fn->String().CompareTo(r->String())==0){
	Error("Run already in list: ",filename) ;
	return ;
      }
    }
    fRunList->Add(fn) ;
  }
  
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::Exec(Option_t * option)
{
  // reads parameters and does the calibration
  ScanPedestals(option);
  CalculatePedestals();             
  WritePedestals();
  ScanGains(option) ;
  CalculateGains() ;
  WriteGains() ;
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::Init(void)
{
  // intializes everything

  //check if ConTableDB already read 
  if(!fctdb){     
    TFile * v = gROOT->GetFile(fConTableDBFile) ;
    if(!v)
      v = TFile::Open(fConTableDBFile) ;
    if(!v){
      Fatal("Can not open file with Connection Table DB:",fConTableDBFile) ;
      return ;
    }  
    fctdb = dynamic_cast<AliPHOSConTableDB *>(v->Get("AliPHOSConTableDB")) ;
  }

  fNch = fctdb->GetNchanels() ;
  fhPedestals   = new TH1F("hPedestals","Pedestals mean",fNch,0.,fNch) ;
  fhPedestalsWid= new TH1F("hPedestalsWid","Pedestals width",fNch,0.,fNch) ;
  fhGains       = new TH1F("hGains","Gains ",fNch,0.,fNch) ; 
  fhGainsWid    = new TH1F("hGainsWid","Gains width",fNch,0.,fNch) ; 
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::SetConTableDB(const char * file,const char * name)
{
  //Reads Connection Table database with name "name" from file "file" 

  if(file==0 || name == 0){
    Error("Please, specify file with database"," and its title") ;
    return ;
  }
  if(fctdb && strcmp(fctdb->GetTitle(),name)==0) //already read
    return ;

  //else read new one
  if(fctdb){
    delete fctdb ;
    fctdb = 0;
  }

  TFile * v = gROOT->GetFile(fConTableDBFile) ;
  if(!v)
    v = TFile::Open(fConTableDBFile) ;
  if(!v){
    Error("Can not open file with Connection Table DB:",fConTableDBFile) ;
    return ;
  }  
  fctdb = dynamic_cast<AliPHOSConTableDB *>(v->Get("AliPHOSConTableDB")) ;
  
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::PlotPedestal(Int_t chanel)
{
  //Plot histogram for a given channel, filled in Scan method
  if(fPedHistos && fPedHistos->GetEntriesFast()){
    static_cast<TH1F*>(fPedHistos->At(chanel))->Draw() ;
  }
  else{
    printf("Histograms not created yet! \n") ;
  } 
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::PlotPedestals(void)
{
  // draws pedestals distribution
  fhPedestals->Draw() ;
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::PlotGain(Int_t chanel)
{
  //Plot histogram for a given channel, filled in Scan method
  if(fGainHistos && fGainHistos->GetEntriesFast()){
    static_cast<TH1F*>(fGainHistos->At(chanel))->Draw() ;
  }
  else{
    printf("Histograms not created yet! \n") ;
  } 
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::PlotGains(void)
{
  // draws gains distribution
  fhGains->Draw() ;
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::ScanPedestals(Option_t * option )
{
  //scan all files in list fRunList and fill pedestal hisgrams
  //option: "clear" - clear pedestal histograms filled up to now
  //        "deb" - plot file name currently processed

  if(!fctdb)
    Init() ;

  if(fPedHistos && strstr(option,"clear"))
    fPedHistos->Delete() ;
  if(!fPedHistos)
    fPedHistos = new TObjArray(fNch) ;

  //Create histos for each channel, fills them and extracts mean values.
  //First - prepare histos
  Int_t ich ;
  for(ich=0;ich<fNch ;ich++){
    TH1F * h = static_cast<TH1F *>(fPedHistos->At(ich)) ;
    if(!h ){
      TString n("hPed");
      n+=ich ;
      TString name("Pedestal for channel ") ;
      name += ich ;
      fPedHistos->AddAt(new TH1F(n,name,fNChan,0,fNChan),ich) ; 
    }
  }

  TIter next(fRunList) ;
  TObjString * file ;
  while((file = static_cast<TObjString *>(next()))){
    if(strstr(option,"deb"))
      printf("Processing file %s \n ",file->String().Data()) ;
    AliPHOSGetter * gime = AliPHOSGetter::Instance(file->String().Data(),GetTitle()) ;
    Int_t ievent ;
    for(ievent = 0; ievent<gime->MaxEvent() ; ievent++){
      gime->Event(ievent,"D") ;
      if(gime->EventPattern() == fPedPat ){
	Int_t idigit ;
	TClonesArray * digits = gime->Digits() ;
	for(idigit = 0; idigit<digits->GetEntriesFast() ;  idigit++){
	  AliPHOSDigit * digit = static_cast<AliPHOSDigit *>(digits->At(idigit) ) ;
	  ich = fctdb->AbsId2Raw(digit->GetId());

	  if(ich>=0){
	    Float_t amp = digit->GetAmp() ;
	    TH1F * hh = dynamic_cast<TH1F*>(fPedHistos->At(ich)) ;
	    hh->Fill(amp) ;
	  }
	}
      }
    }
  }
   
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::CalculatePedestals()
{
  //Fit histograms, filled in ScanPedestals method with Gaussian
  //find mean and width, check deviation from mean for each channel.

  if(!fPedHistos || !fPedHistos->At(0)){
    Error("CalculatePedestals","You should run ScanPedestals first!") ;
    return ;
  }

  //Now fit results with Gauss
  TF1 * gs = new TF1("gs","gaus",0.,10000.) ;
  Int_t ich ;
  for(ich=0;ich<fNch ;ich++){
    TH1F * h = static_cast<TH1F *>(fPedHistos->At(ich)) ;
    Int_t max = h->GetMaximumBin() ;
    Axis_t xmin = max/2. ;
    Axis_t xmax = max*3/2 ;
    gs->SetRange(xmin,xmax) ;
    Double_t par[3] ;
    par[0] = h->GetBinContent(max) ;
    par[1] = max ;
    par[2] = max/3 ;
    gs->SetParameters(par[0],par[1],par[2]) ;
    h->Fit("gs","QR") ;
    gs->GetParameters(par) ;
    fhPedestals->SetBinContent(ich,par[1]) ;
    fhPedestals->SetBinError(ich,par[2]) ;
    fhPedestalsWid->Fill(ich,par[2]) ;
  }
  delete gs ;

  //now check reasonability of results
  TF1 * p0 = new TF1("p0","pol0",0.,fNch) ;
  fhPedestals->Fit("p0","Q") ;
  Double_t meanPed ;
  p0->GetParameters(&meanPed);
  for(ich=0;ich<fNch ;ich++){
    Float_t ped =  fhPedestals->GetBinContent(ich) ;
    if(ped < 0  || ped > meanPed+fAcceptCorr){
      TString out("Pedestal of channel ") ;
      out+=ich ;     
      out+=" is ";
      out+= ped ;
      out+= "it is too far from mean " ;
      out+= meanPed ;
      Error("PHOSCalibrator",out) ;
    }
  }
  delete p0 ;
    
}
//____________________________________________________________________________ 
void AliPHOSCalibrator::ScanGains(Option_t * option)
{
  //Scan all runs, listed in fRunList and fill histograms for all channels
  //options: "clear" - clean histograms, filled up to now
  //         "deb" - print current file name
  //         "narrow" - scan only narrow beam events

  if(!fctdb)
    Init() ;
  if(fGainHistos && strstr(option,"clear"))
    fGainHistos->Delete() ;
  if(!fGainHistos){
    if(strstr(option,"deball"))
      printf("creating array for %d channels \n",fNch) ;	    
    fGainHistos   = new TObjArray(fNch) ;
  }

  //Create histos for each channel, fills them and extracts mean values.
  //First - prepare  histos

  if(!fGainHistos->GetEntriesFast()){
    Int_t ich ;
    for(ich=0;ich<fNch ;ich++){   
      TString n("hGain");
      n+=ich ;
      TString name("Gains for channel ") ;
      name += ich ;
      fGainHistos->AddAt(new TH1F(n,name,fNGainBins,0,fGainMax),ich) ; 
      //      static_cast<TH1F*>(fGainHistos->At(ich))->Sumw2() ;
    }
  }

  Bool_t all =!(Bool_t)strstr(option,"narrow") ;


  TIter next(fRunList) ;
  TObjString * file ;
  while((file = static_cast<TObjString *>(next()))){
    
    AliPHOSGetter * gime = AliPHOSGetter::Instance(file->String().Data(),GetTitle()) ;
    Int_t handled = 0;
    Int_t ievent ;
    for(ievent = 0; ievent<gime->MaxEvent() ; ievent++){
      gime->Event(ievent,"D") ;
      if(gime->EventPattern() == fNBPat || 
	 (all && gime->EventPattern() == fWBPat)){
	handled ++ ;
	Int_t idigit ;
	TClonesArray * digits = gime->Digits() ;
	AliPHOSDigit * digit ;
	Int_t max = 0 ;
	Int_t imax = 0;
	for(idigit = 0; idigit<digits->GetEntriesFast() ;  idigit++){
	  digit = static_cast<AliPHOSDigit *>(digits->At(idigit) ) ;
	  if(digit->GetAmp() > max){
	    imax = idigit ;
	    max = digit->GetAmp() ;
	  }
	}
	digit = static_cast<AliPHOSDigit *>(digits->At(imax) ) ;
	Int_t ich = fctdb->AbsId2Raw(digit->GetId());
	if(ich>=0){
	  Float_t pedestal = fhPedestals->GetBinContent(ich) ;
	  const Float_t kshowerInCrystall = 0.9 ;
	  Float_t beamEnergy = gime->BeamEnergy() ;
	  Float_t gain = beamEnergy*kshowerInCrystall/
		         (digit->GetAmp() - pedestal) ;
	  static_cast<TH1F*>(fGainHistos->At(ich))->Fill(gain) ;
	} 
      }
    }
    if(strstr(option,"deb"))
      printf("Hadled %d events \n",handled) ;
  }
}   
//____________________________________________________________________________ 
void AliPHOSCalibrator::CalculateGains(void)
{
  //calculates gain

  if(!fGainHistos || !fGainHistos->GetEntriesFast()){
    Error("CalculateGains","You should run ScanGains first!") ; 
    return ;
  }

  //Fit results with Landau
  TF1 * gs = new TF1("gs","landau",0.,10000.) ;
  Int_t ich ;
  for(ich=0;ich<fNch ;ich++){
    TH1F * h = static_cast<TH1F *>(fGainHistos->At(ich)) ;
    Int_t bmax = h->GetMaximumBin() ;
    Axis_t center = h->GetBinCenter(bmax) ;
    Axis_t xmin = center - 0.01 ;
    Axis_t xmax = center + 0.02 ;
    gs->SetRange(xmin,xmax) ;
    Double_t par[3] ;
    par[0] = h->GetBinContent(bmax) ;
    par[1] = center ;
    par[2] = 0.001 ;
    gs->SetParameters(par[0],par[1],par[2]) ;
    h->Fit("gs","QR") ;
    gs->GetParameters(par) ;
    fhGains->SetBinContent(ich,par[1]) ;
    fhGains->SetBinError(ich,par[2]) ;
    fhGainsWid->Fill(ich,par[2]) ;
  }
  delete gs ;

  //now check reasonability of results
  TF1 * p0 = new TF1("p0","pol0",0.,fNch) ;
  fhGains->Fit("p0","Q") ;
  Double_t meanGain ;
  p0->GetParameters(&meanGain);
  for(ich=0;ich<fNch ;ich++){
    Float_t gain =  fhGains->GetBinContent(ich) ;
    if(gain < meanGain/fGainAcceptCorr  || gain > meanGain*fGainAcceptCorr){
      TString out("Gain of channel ") ;
      out+=ich ;     
      out+=" is ";
      out+= gain ;
      out+= "it is too far from mean " ;
      out+= meanGain ;
      Error("PHOSCalibrator",out) ;
    }
  }
  delete p0 ;
    
}

//_____________________________________________________________________________
void AliPHOSCalibrator::WritePedestals(const char * version,
		                         Int_t begin,Int_t end)
{
  //Write calculated data to file using AliPHOSCalibrManager
  //version and validitirange (begin-end) will be used to identify data 

  if(!fctdb){
    Error("WritePedestals","\n           Please, supply Connection Table DB (use SetConTableDB()) \n" ) ;
    return ;
  }
  //fill data	
  AliPHOSCalibrationData ped("Pedestals",version);
  for(Int_t i=0; i<fNch;i++){
    Int_t absid=fctdb->Raw2AbsId(i) ;
    ped.SetData(absid,fhPedestals->GetBinContent(i)) ;
    ped.SetDataCheck(absid,fhPedestalsWid->GetBinContent(i)) ;
  }

  //evaluate validity range
  if(begin==0){
    TIter next(fRunList) ;
    Int_t ibegin=99999;
    Int_t iend=0 ;
    TObjString * file ;
    while((file=((TObjString*)next()))){
       TString s = file->GetString() ;
       TString ss = s(s.Last('_'),s.Last('.'));
       Int_t tmp ;
       if(sscanf(ss.Data(),"%d",&tmp)){
	 if(ibegin<tmp)
	   ibegin=tmp ;	 
         if(iend>tmp)
	   iend=tmp ;
       }
    } 
    ped.SetValidityRange(ibegin,iend) ;
  }
  else	  
    ped.SetValidityRange(begin,end) ;

  //check, may be Manager instance already configured?
  AliPHOSCalibrManager * cmngr = AliPHOSCalibrManager::GetInstance() ;
  if(!cmngr){
    Warning("Write Pedestals","Using database file 'PHOSBTCalibration.root'") ;
    cmngr = AliPHOSCalibrManager::GetInstance("PHOSBTCalibration.root") ;
  }
  cmngr->WriteData(&ped) ;
}	
//_____________________________________________________________________________
void AliPHOSCalibrator::ReadPedestals(const char * version,
		                         Int_t range)
{ 
  //Read data from file using AliPHOSCalibrManager 
  //version and range will be used to choose proper data

  AliPHOSCalibrationData ped("Pedestals",version);
  AliPHOSCalibrManager * cmngr = AliPHOSCalibrManager::GetInstance() ;
  if(!cmngr){
   Warning("ReadPedestals","Using database file 'PHOSBTCalibration.root'") ;
   cmngr = AliPHOSCalibrManager::GetInstance("PHOSBTCalibration.root") ;
  }
  cmngr->ReadFromRoot(ped,range) ;
  Int_t npeds=ped.NChannels() ;
  fNch = fctdb->GetNchanels() ;
  if(fhPedestals)
    delete fhPedestals ;
  fhPedestals   = new TH1F("hPedestals","Pedestals mean",fNch,0.,fNch) ;
  for(Int_t i=0;i<npeds;i++){
    Int_t raw =fctdb->AbsId2Raw(i) ;
    if(raw){
      fhPedestals->SetBinContent(raw-1,ped.Data(i)) ;
      fhPedestals->SetBinError(raw-1,ped.DataCheck(i)) ;
    }
  }
}	
//_____________________________________________________________________________
void AliPHOSCalibrator::ReadGains(const char * version,
		                         Int_t range)
{ 
  //Read data from file using AliPHOSCalibrManager 
  //version and range will be used to choose proper data

  AliPHOSCalibrationData gains("Gains",version);
  AliPHOSCalibrManager * cmngr = AliPHOSCalibrManager::GetInstance() ;
  if(!cmngr){
    Warning("ReadGainss","Using database file 'PHOSBTCalibration.root'") ;
    cmngr = AliPHOSCalibrManager::GetInstance("PHOSBTCalibration.root") ;
  }
  cmngr->ReadFromRoot(gains,range) ;
  Int_t npeds=gains.NChannels() ;
  fNch = fctdb->GetNchanels() ;
  if(fhGains)
    delete fhGains ;
  fhGains   = new TH1F("hGainss","Gains mean",fNch,0.,fNch) ;
  for(Int_t i=0;i<npeds;i++){
    Int_t raw =fctdb->AbsId2Raw(i) ;
    if(raw){
      fhGains->SetBinContent(raw-1,gains.Data(i)) ;
      fhGains->SetBinError(raw-1,gains.DataCheck(i)) ;
    }
  }
}	
//_____________________________________________________________________________
void AliPHOSCalibrator::WriteGains(const char * version,
				     Int_t begin,Int_t end)
{ 
  //Write gains through AliPHOSCalibrManager
  //version and validity range(begin-end) are used to identify data

  if(!fctdb){
    Error("WriteGains","\n        Please, supply Connection Table DB (use SetConTableDB()) \n" ) ;
    return ;
  }

  AliPHOSCalibrationData gains("Gains",version);
  for(Int_t i=0; i<fNch;i++){
    Int_t absid=fctdb->Raw2AbsId(i) ;
    gains.SetData(absid,fhGains->GetBinContent(i)) ;
    gains.SetDataCheck(absid,fhGainsWid->GetBinContent(i)) ;
  }
  if(begin==0){
    TIter next(fRunList) ;
    Int_t ibegin=99999;
    Int_t iend=0 ;
    TObjString * file ;
    while((file=((TObjString*)next()))){
       TString s = file->GetString() ;
       TSubString ss = s(s.Last('_'),s.Last('.'));
       Int_t tmp ;
       if(sscanf(ss.Data(),"%d",&tmp)){
	 if(ibegin<tmp)
	   ibegin=tmp ;	 
         if(iend>tmp)
	   iend=tmp ;
       }
    } 
    gains.SetValidityRange(ibegin,iend) ;
  }
  else	  
    gains.SetValidityRange(begin,end) ;
  AliPHOSCalibrManager * cmngr = AliPHOSCalibrManager::GetInstance() ;
  if(!cmngr){
    Warning("WriteGains","Using database file 'PHOSBTCalibration.root'") ;
    cmngr = AliPHOSCalibrManager::GetInstance("PHOSBTCalibration.root") ;
  }
  cmngr->WriteData(&gains) ;
}	
//_____________________________________________________________________________
void AliPHOSCalibrator::Print(const Option_t * option)const 
{
  // prints everything
  printf("--------------AliPHOSCalibrator-----------------\n") ;
  printf("Files to handle:\n") ;
  TIter next(fRunList) ;
  TObjString * r ;
  while((r=(TObjString *)(next())))
      printf("                %s\n",r->GetName()) ;

  printf("Name of ConTableDB:.....................%s\n",fConTableDB.Data()) ;
  printf("File of ConTableDB:.....................%s\n",fConTableDBFile.Data() ) ;
  printf("Maximal deviation from mean Gain (factor):.%f\n",fGainAcceptCorr) ;
  printf("Maximal deviation of Pedestal from mean:...%f\n",fAcceptCorr) ; 
  printf("Range used in Gain histos:..............%f\n",fGainMax) ;
  printf("Number of bins in Gain histos:..........%d\n",fNGainBins) ;
  printf("Number of channels to calibrate:........%d\n",fNch) ;
  printf("Number of bins in pedestal histos:......%d\n",fNChan) ;
  printf("trigger pattern for PEDESTAL events:....%d\n",fPedPat) ;
  printf("trigger pattern for PULSER events:......%d\n",fPulPat) ;
  printf("trigger pattern for LED events:.........%d\n",fLEDPat) ;
  printf("trigger pattern for WIDE BEAM events:...%d\n",fWBPat) ;
  printf("trigger pattern for NARROW BEAM events:.%d\n",fNBPat) ;
  printf("--------------------------------------------------\n") ;
}

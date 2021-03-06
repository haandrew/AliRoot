//____________________________________________________________________
//
// $Id$
//
// Draw hits in the specialised FMD event fancy 
//
/** Fancy hits 
    @ingroup FMD_script
 */
void
FancyDigits()
{
  AliCDBManager* cdb = AliCDBManager::Instance();
  cdb->SetDefaultStorage("local://$ALICE_ROOT/OCDB");
  gSystem->Load("libFMDutil");
  AliFMDFancy* d = new AliFMDFancy;
  d->AddLoad(AliFMDInput::kDigits);
  // d->AddLoad(AliFMDInput::kKinematics);
  d->Run();
}

//____________________________________________________________________
//
// EOF
//

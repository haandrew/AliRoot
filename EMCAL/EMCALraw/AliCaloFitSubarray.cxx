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

#include "AliCaloFitSubarray.h"

///
/// Constructor
//_____________________________________________________________________
AliCaloFitSubarray::AliCaloFitSubarray(const Int_t bunchIndex, 
                                       const Int_t maxrev, 
                                       const Int_t first, 
                                       const Int_t last ) : 
fBunchIndex(bunchIndex),
fMaxRev(maxrev), 
fFirst(first), 
fLast(last)   
{
}

///
/// Constructor
//_____________________________________________________________________
AliCaloFitSubarray::AliCaloFitSubarray(const Int_t init) : 
fBunchIndex(init),
fMaxRev(init), 
fFirst(init), 
fLast(init)   
{
}

///
/// Copy constructor
//_____________________________________________________________________
AliCaloFitSubarray::AliCaloFitSubarray(const AliCaloFitSubarray & fitS) :
fBunchIndex( fitS.fBunchIndex ),
fMaxRev( fitS.fMaxRev ), 
fFirst( fitS.fFirst ), 
fLast( fitS.fLast )   
{
}

///
/// Assignment operator; use copy ctor
//_____________________________________________________________________
AliCaloFitSubarray& AliCaloFitSubarray::operator = (const AliCaloFitSubarray &source)
{
  if (&source == this) return *this;
  
  new (this) AliCaloFitSubarray(source);
  return *this;
}

///
/// Destructor
//_____________________________________________________________________
AliCaloFitSubarray::~AliCaloFitSubarray()
{
}


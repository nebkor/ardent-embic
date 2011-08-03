//-*****************************************************************************
//
// Copyright (c) 2009-2011,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <string.h>
#include <sstream>
#include <time.h>

#include <Alembic/AbcCoreAbstract/Foundation.h>
#include <Alembic/AbcCoreAbstract/MetaData.h>
#include <Alembic/AbcCoreAbstract/ArchiveWriter.h>
#include <Alembic/AbcCoreAbstract/ArchiveReader.h>

namespace Alembic {
namespace AbcCoreAbstract {
namespace ALEMBIC_VERSION_NS {


// 11/2/2010: Alembic 0.9      
// 2/23/2011: Alembic 0.92 
// 5/18/2011: Alembic 0.93    
// 5/25/2011: Alembic 0.93b 
// 6/29/2011: Alembic 1.0.rc1 
// 8/8/2011:  Alembic 1.0.0 
// This symbol's name gives a meaningful link / dlopen error
// message to people if they mismatch plugins to library.
const char *    kAlembicVersionStringALEMBIC_API_VERSION_1_0rc2 = "1.0.rc2";
static const char * _kAlembicVersionString = 
	kAlembicVersionStringALEMBIC_API_VERSION_1_0rc2;

std::string 
GetLibraryVersionShort()
{
    std::string versionString (_kAlembicVersionString);
    return versionString;
}

std::string 
GetLibraryVersion()
{
    // "Alembic 1.0.0 (7/6/2011)"
    const char * date = __DATE__;
    std::string   alembicVersion = GetLibraryVersionShort();
    std::ostringstream sversionString;
    sversionString << "Alembic " << alembicVersion 
        << " (built " << date << ")";
 
    return sversionString.str ();
}

} // End namespace ALEMBIC_VERSION_NS
} // End namespace AbcCoreAbstract
} // End namespace Alembic

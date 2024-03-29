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

#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/Util/All.h>

#include <Alembic/AbcCoreHDF5/Tests/Assert.h>

#include <iostream>
#include <vector>

#include <hdf5.h>

//-*****************************************************************************
namespace A5 = Alembic::AbcCoreHDF5;

namespace ABC = Alembic::AbcCoreAbstract::v1;

using namespace Alembic::Util;

//-*****************************************************************************
void testDuplicateArray()
{
    std::string archiveName = "repeatArray.abc";

    size_t numVals = 35;

    {
        A5::WriteArchive w;
        ABC::ArchiveWriterPtr a = w(archiveName, ABC::MetaData());
        ABC::ObjectWriterPtr archive = a->getTop();

        ABC::CompoundPropertyWriterPtr parent = archive->getProperties();

        ABC::ArrayPropertyWriterPtr awp =
            parent->createArrayProperty("a",  ABC::MetaData(),
                                        ABC::DataType(Alembic::Util::kInt16POD, 1), 0);

        ABC::ArrayPropertyWriterPtr bwp =
            parent->createArrayProperty("b", ABC::MetaData(),
                                        ABC::DataType(Alembic::Util::kInt16POD, 1), 0);

        ABC::ArrayPropertyWriterPtr cwp =
            parent->createArrayProperty("c", ABC::MetaData(),
                                        ABC::DataType(Alembic::Util::kInt8POD, 1), 0);

        Alembic::Util::Dimensions dims(numVals);
        ABC::DataType i16d(Alembic::Util::kInt16POD, 1);
        std::vector <Alembic::Util::int16_t> vali(numVals, 0);
        vali[0] = 7;
        vali[1] = 7;
        vali[2] = 7;

        awp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        vali[0] = 8;
        vali[1] = 16;
        vali[2] = 32;
        awp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        vali[0] = 7;
        vali[1] = 7;
        vali[2] = 7;
        awp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        // lets write b and make sure it shares
        vali[0] = 8;
        vali[1] = 16;
        vali[2] = 32;
        bwp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        vali[0] = 7;
        vali[1] = 7;
        vali[2] = 7;
        bwp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        vali[0] = 8;
        vali[1] = 16;
        vali[2] = 32;
        bwp->setSample(ABC::ArraySample(&(vali.front()), i16d, dims));

        // c shouldn't share because it's pod type and extent is different
        std::vector <Alembic::Util::int8_t> vals(numVals*2);
        ABC::DataType i8d(Alembic::Util::kInt8POD, 1);

        vals[0] = 8;
        vals[1] = 0;
        vals[2] = 16;
        vals[3] = 0;
        vals[4] = 32;
        vals[5] = 0;
        dims = Alembic::Util::Dimensions(numVals*2);
        cwp->setSample(ABC::ArraySample(&(vals.front()), i8d, dims));


        vals[0] = 7;
        vals[1] = 0;
        vals[2] = 7;
        vals[3] = 0;
        vals[4] = 7;
        vals[5] = 0;
        cwp->setSample(ABC::ArraySample(&(vals.front()), i8d, dims));
        cwp->setSample(ABC::ArraySample(&(vals.front()), i8d, dims));
    }

    {
        A5::ReadArchive r;
        ABC::ArchiveReaderPtr a = r( archiveName );
        ABC::ObjectReaderPtr archive = a->getTop();
        ABC::CompoundPropertyReaderPtr parent = archive->getProperties();

        TESTING_ASSERT(parent->getNumProperties() == 3);
        for (size_t i = 0; i < parent->getNumProperties(); ++i)
        {
            ABC::BasePropertyReaderPtr bp = parent->getProperty( i );
            TESTING_ASSERT(bp->isArray());
            ABC::ArrayPropertyReaderPtr ap = bp->asArrayPtr();
            TESTING_ASSERT(ap->getNumSamples() == 3);


            if (ap->getName() == "a")
            {
                Dimensions dims0;
                Dimensions dims1;
                Dimensions dims2;
                ap->getDimensions(0, dims0);
                ap->getDimensions(1, dims1);
                ap->getDimensions(2, dims2);

                TESTING_ASSERT(dims0.numPoints() == numVals);
                TESTING_ASSERT(dims1.numPoints() == numVals);
                TESTING_ASSERT(dims2.numPoints() == numVals);

                ABC::ArraySamplePtr samp0;
                ABC::ArraySamplePtr samp1;
                ABC::ArraySamplePtr samp2;
                ap->getSample(0, samp0);
                ap->getSample(1, samp1);
                ap->getSample(2, samp2);

                TESTING_ASSERT(samp0->getDimensions().numPoints() == numVals);
                TESTING_ASSERT(samp1->getDimensions().numPoints() == numVals);
                TESTING_ASSERT(samp2->getDimensions().numPoints() == numVals);

                int16_t * data = (int16_t *)(samp0->getData());
                TESTING_ASSERT(data[0] == 7);
                TESTING_ASSERT(data[1] == 7);
                TESTING_ASSERT(data[2] == 7);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int16_t *)(samp1->getData());
                TESTING_ASSERT(data[0] == 8);
                TESTING_ASSERT(data[1] == 16);
                TESTING_ASSERT(data[2] == 32);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int16_t *)(samp2->getData());
                TESTING_ASSERT(data[0] == 7);
                TESTING_ASSERT(data[1] == 7);
                TESTING_ASSERT(data[2] == 7);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }
            }
            else if (ap->getName() == "b")
            {
                ABC::ArraySamplePtr samp0;
                ABC::ArraySamplePtr samp1;
                ABC::ArraySamplePtr samp2;
                ap->getSample(0, samp0);
                ap->getSample(1, samp1);
                ap->getSample(2, samp2);

                TESTING_ASSERT(samp0->getDimensions().numPoints() == numVals);
                TESTING_ASSERT(samp1->getDimensions().numPoints() == numVals);
                TESTING_ASSERT(samp2->getDimensions().numPoints() == numVals);

                Dimensions dims0;
                Dimensions dims1;
                Dimensions dims2;
                ap->getDimensions(0, dims0);
                ap->getDimensions(1, dims1);
                ap->getDimensions(2, dims2);

                TESTING_ASSERT(dims0.numPoints() == numVals);
                TESTING_ASSERT(dims1.numPoints() == numVals);
                TESTING_ASSERT(dims2.numPoints() == numVals);

                int16_t * data = (int16_t *)(samp0->getData());
                TESTING_ASSERT(data[0] == 8);
                TESTING_ASSERT(data[1] == 16);
                TESTING_ASSERT(data[2] == 32);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int16_t *)(samp1->getData());
                TESTING_ASSERT(data[0] == 7);
                TESTING_ASSERT(data[1] == 7);
                TESTING_ASSERT(data[2] == 7);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int16_t *)(samp2->getData());
                TESTING_ASSERT(data[0] == 8);
                TESTING_ASSERT(data[1] == 16);
                TESTING_ASSERT(data[2] == 32);
                for (size_t j = 3; j < numVals; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }
            }
            else if (ap->getName() == "c")
            {
                ABC::ArraySamplePtr samp0;
                ABC::ArraySamplePtr samp1;
                ABC::ArraySamplePtr samp2;
                ap->getSample(0, samp0);
                ap->getSample(1, samp1);
                ap->getSample(2, samp2);

                TESTING_ASSERT(samp0->getDimensions().numPoints() == numVals*2);
                TESTING_ASSERT(samp1->getDimensions().numPoints() == numVals*2);
                TESTING_ASSERT(samp2->getDimensions().numPoints() == numVals*2);

                Dimensions dims0;
                Dimensions dims1;
                Dimensions dims2;
                ap->getDimensions(0, dims0);
                ap->getDimensions(1, dims1);
                ap->getDimensions(2, dims2);

                TESTING_ASSERT(dims0.numPoints() == numVals*2);
                TESTING_ASSERT(dims1.numPoints() == numVals*2);
                TESTING_ASSERT(dims2.numPoints() == numVals*2);

                int8_t * data = (int8_t *)(samp0->getData());
                TESTING_ASSERT(data[0] == 8);
                TESTING_ASSERT(data[1] == 0);
                TESTING_ASSERT(data[2] == 16);
                TESTING_ASSERT(data[3] == 0);
                TESTING_ASSERT(data[4] == 32);
                TESTING_ASSERT(data[5] == 0);
                for (size_t j = 6; j < numVals*2; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int8_t *)(samp1->getData());
                TESTING_ASSERT(data[0] == 7);
                TESTING_ASSERT(data[1] == 0);
                TESTING_ASSERT(data[2] == 7);
                TESTING_ASSERT(data[3] == 0);
                TESTING_ASSERT(data[4] == 7);
                TESTING_ASSERT(data[5] == 0);
                for (size_t j = 6; j < numVals*2; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }

                data = (int8_t *)(samp2->getData());
                TESTING_ASSERT(data[0] == 7);
                TESTING_ASSERT(data[1] == 0);
                TESTING_ASSERT(data[2] == 7);
                TESTING_ASSERT(data[3] == 0);
                TESTING_ASSERT(data[4] == 7);
                TESTING_ASSERT(data[5] == 0);
                for (size_t j = 6; j < numVals*2; ++j)
                {
                    TESTING_ASSERT(data[j] == 0);
                }
            }
        }
    }
}

//-*****************************************************************************
void testReadWriteArrays()
{

    std::string archiveName = "arrayProperties.abc";

    {
        A5::WriteArchive w;
        ABC::ArchiveWriterPtr a = w(archiveName, ABC::MetaData());
        ABC::ObjectWriterPtr archive = a->getTop();

        ABC::CompoundPropertyWriterPtr props = archive->getProperties();

        {
            ABC::DataType dtype(Alembic::Util::kBooleanPOD, 1);
            ABC::ArrayPropertyWriterPtr boolWrtPtr =
                props->createArrayProperty("bool", ABC::MetaData(), dtype, 0 );


            std::vector < Alembic::Util::bool_t > vals(3);
            vals[0] = false;
            vals[1] = true;
            vals[2] = false;
            Alembic::Util::Dimensions dims(vals.size());
            boolWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }


        {
            ABC::DataType dtype(Alembic::Util::kUint8POD, 1);
            ABC::ArrayPropertyWriterPtr uint8WrtPtr =
                props->createArrayProperty("uint8", ABC::MetaData(), dtype, 0);

            std::vector< Alembic::Util::uint8_t > vals(4);
            vals[0] = 200;
            vals[1] = 45;
            vals[2] = 37;
            vals[3] = 192;

            TESTING_ASSERT(uint8WrtPtr->getNumSamples() == 0);
            Alembic::Util::Dimensions dims(vals.size());
            uint8WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
            TESTING_ASSERT(uint8WrtPtr->getNumSamples() == 1);
        }

        {
            ABC::DataType dtype(Alembic::Util::kUint8POD, 1);
            ABC::ArrayPropertyWriterPtr uint8WrtPtr =
                props->createArrayProperty("uint8_newDims",
                                           ABC::MetaData(), dtype, 0);

            std::vector< Alembic::Util::uint8_t > vals(4);
            vals[0] = 200;
            vals[1] = 45;
            vals[2] = 37;
            vals[3] = 192;

            TESTING_ASSERT(uint8WrtPtr->getNumSamples() == 0);
            Alembic::Util::Dimensions dims;
            dims.setRank(2);
            dims[0] = 2;
            dims[1] = 2;
            uint8WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
            TESTING_ASSERT(uint8WrtPtr->getNumSamples() == 1);
        }

        {
            ABC::DataType dtype(Alembic::Util::kInt8POD, 1);
            ABC::ArrayPropertyWriterPtr charWrtPtr =
                props->createArrayProperty("int8",  ABC::MetaData(), dtype, 0 );

            std::vector < Alembic::Util::int8_t > vals(2);
            vals[0] = -20;
            vals[1] = 45;

            Alembic::Util::Dimensions dims(vals.size());
            charWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kUint16POD, 1);
            ABC::ArrayPropertyWriterPtr uint16WrtPtr =
                props->createArrayProperty("uint16", ABC::MetaData(),
                                           ABC::DataType(Alembic::Util::kUint16POD, 1), 0 );

            std::vector < Alembic::Util::uint16_t > vals(3);
            vals[0] = 60000;
            vals[1] = 2;
            vals[2] = 3987;

            Alembic::Util::Dimensions dims(vals.size());
            uint16WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));

        }

        {
            ABC::DataType dtype(Alembic::Util::kInt16POD, 1);
            ABC::ArrayPropertyWriterPtr int16WrtPtr =
                props->createArrayProperty("int16", ABC::MetaData(),
                                           dtype, 0);
            std::vector < int16_t > vals(2);
            vals[0] = -20000;
            vals[1] = 77;
            Alembic::Util::Dimensions dims(vals.size());
            int16WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kUint32POD, 1);
            ABC::ArrayPropertyWriterPtr uint32WrtPtr =
                props->createArrayProperty("uint32",  ABC::MetaData(), dtype,
                                           0);

            std::vector < Alembic::Util::uint32_t > vals(1);
            vals[0] = 1000000;

            Alembic::Util::Dimensions dims(vals.size());
            uint32WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kInt32POD, 1);
            ABC::ArrayPropertyWriterPtr int32WrtPtr =
                props->createArrayProperty("int32", ABC::MetaData(), dtype, 0);
            std::vector < Alembic::Util::int32_t > vals(4);
            vals[0] = -1000000;
            vals[1] = 42;
            vals[2] = -20000;
            vals[3] = 123456;

            Alembic::Util::Dimensions dims(vals.size());
            int32WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kUint64POD, 1);
            ABC::ArrayPropertyWriterPtr ui64WrtPtr =
                props->createArrayProperty("uint64", ABC::MetaData(), dtype, 0);
            std::vector < Alembic::Util::uint64_t > vals(3);
            vals[0] = 5000000000LL;
            vals[1] = 1234567891011LL;
            vals[2] = 12;

            Alembic::Util::Dimensions dims(vals.size());
            ui64WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kInt64POD, 1);
            ABC::ArrayPropertyWriterPtr i64WrtPtr =
                props->createArrayProperty("int64", ABC::MetaData(), dtype, 0);

            std::vector < Alembic::Util::int64_t > vals(2);
            vals[0] = -5000000000LL;
            vals[1] = 9876543210LL;
            Alembic::Util::Dimensions dims(vals.size());
            i64WrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kFloat16POD, 1);
            ABC::ArrayPropertyWriterPtr halfWrtPtr =
                props->createArrayProperty("float16",
                                           ABC::MetaData(), dtype, 0);

            std::vector < Alembic::Util::float16_t > vals(2);
            vals[0] = 16.0;
            vals[1] = -3.0;

            Alembic::Util::Dimensions dims(vals.size());
            halfWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kFloat32POD, 2);
            ABC::ArrayPropertyWriterPtr floatWrtPtr =
                props->createArrayProperty("float32", ABC::MetaData(), dtype,
                                           0);
            std::vector < Alembic::Util::float32_t > vals(4);
            vals[0] = 128.0;
            vals[1] = -13.25;
            vals[2] = 35.5;
            vals[3] = 128.125;
            Alembic::Util::Dimensions dims(vals.size()/2);
            floatWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kFloat32POD, 1);
            ABC::ArrayPropertyWriterPtr floatWrtPtr =
                props->createArrayProperty("float32_ext1",
                                           ABC::MetaData(), dtype, 0);
            std::vector < Alembic::Util::float32_t > vals(4);
            vals[0] = 128.0;
            vals[1] = -13.25;
            vals[2] = 35.5;
            vals[3] = 128.125;
            Alembic::Util::Dimensions dims(vals.size());
            floatWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kFloat64POD, 1);
            ABC::ArrayPropertyWriterPtr doubleWrtPtr =
                props->createArrayProperty("float64",  ABC::MetaData(),
                                           dtype, 0);

            std::vector < Alembic::Util::float64_t > vals(30);
            for (size_t i = 0; i < vals.size(); ++i)
                vals[i] = i * 100.0;
            TESTING_ASSERT(doubleWrtPtr->getNumSamples() == 0);
            Alembic::Util::Dimensions dims(vals.size());
            doubleWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
            TESTING_ASSERT(doubleWrtPtr->getNumSamples() == 1);
        }

        {
            ABC::DataType dtype(Alembic::Util::kFloat64POD, 3);
            ABC::ArrayPropertyWriterPtr doubleWrtPtr =
                props->createArrayProperty("float64_ext3",
                                           ABC::MetaData(), dtype, 0);

            std::vector < Alembic::Util::float64_t > vals(30);
            for (size_t i = 0; i < vals.size(); ++i)
                vals[i] = i * 100.0;
            TESTING_ASSERT(doubleWrtPtr->getNumSamples() == 0);
            Alembic::Util::Dimensions dims(vals.size() / 3);
            doubleWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
            TESTING_ASSERT(doubleWrtPtr->getNumSamples() == 1);
        }

        {
            ABC::DataType dtype(Alembic::Util::kStringPOD, 1);
            ABC::ArrayPropertyWriterPtr strWrtPtr =
                props->createArrayProperty("str",
                                           ABC::MetaData(), dtype, 0);

            std::vector < Alembic::Util::string > vals(4);
            vals[0] = "Now it's time";
            vals[1] = "";
            vals[2] = "for";
            vals[3] = "cake!";
            Alembic::Util::Dimensions dims(vals.size());
            strWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

        {
            ABC::DataType dtype(Alembic::Util::kWstringPOD, 1);
            ABC::ArrayPropertyWriterPtr wstrWrtPtr =
                props->createArrayProperty("wstr", ABC::MetaData(), dtype, 0);

            std::vector < Alembic::Util::wstring > vals(4);
            vals[0] = L"We do what we must ";
            vals[1] = L"because we can.";
            vals[2] = L"";
            vals[3] = L"\uf8e4 \uf8e2 \uf8d3";
            Alembic::Util::Dimensions dims(vals.size());
            wstrWrtPtr->setSample(
                ABC::ArraySample(&(vals.front()), dtype, dims));
        }

    }

    // now we read what we've written
    {
        A5::ReadArchive r;
        ABC::ArchiveReaderPtr a = r( archiveName );
        ABC::ObjectReaderPtr archive = a->getTop();
        ABC::CompoundPropertyReaderPtr parent = archive->getProperties();

        TESTING_ASSERT(parent->getNumProperties() == 17);

        ABC::ArraySampleKey key;
        for ( size_t i = 0; i < parent->getNumProperties(); ++i )
        {
            ABC::BasePropertyReaderPtr bp = parent->getProperty( i );

            // they are all supposed to be arrays
            TESTING_ASSERT( bp->isArray() );

            ABC::ArrayPropertyReaderPtr ap = bp->asArrayPtr();
            TESTING_ASSERT( ap->getNumSamples() == 1 );
            TESTING_ASSERT( ap->isConstant() );
            TESTING_ASSERT( ap->getParent() == parent);

            if (ap->getName() != "float64_ext3" && ap->getName() != "float32")
                TESTING_ASSERT( ap->getDataType().getExtent() == 1);

            switch (ap->getDataType().getPod())
            {
                case Alembic::Util::kBooleanPOD:
                {
                    TESTING_ASSERT(ap->getName() == "bool");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 3);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);

                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.numPoints() == 3);
                    TESTING_ASSERT(dims0.rank() == 1);

                    Alembic::Util::bool_t * data =
                        (Alembic::Util::bool_t *)(val->getData());
                    TESTING_ASSERT(data[0] == false);
                    TESTING_ASSERT(data[1] == true);
                    TESTING_ASSERT(data[2] == false);

                    TESTING_ASSERT(ap->getKey(0, key));
                    TESTING_ASSERT(key.numBytes == 3);
                    TESTING_ASSERT(key.origPOD == Alembic::Util::kBooleanPOD);
                    TESTING_ASSERT(key.readPOD == Alembic::Util::kBooleanPOD);
                    TESTING_ASSERT(key.digest.str() ==
                        "bbde8ffe4b7e061c7e03081c2bf184c4");
                }
                break;

                case Alembic::Util::kUint8POD:
                {
                    TESTING_ASSERT(ap->getName() == "uint8" ||
                        ap->getName() == "uint8_newDims");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);

                    Alembic::Util::uint8_t * data =
                        (Alembic::Util::uint8_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 200);
                    TESTING_ASSERT(data[1] == 45);
                    TESTING_ASSERT(data[2] == 37);
                    TESTING_ASSERT(data[3] == 192);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 4);
                    TESTING_ASSERT(ap->getKey(0, key));
                    TESTING_ASSERT(key.numBytes == 4);
                    TESTING_ASSERT(key.origPOD == Alembic::Util::kUint8POD);
                    TESTING_ASSERT(key.readPOD == Alembic::Util::kUint8POD);
                    TESTING_ASSERT(key.digest.str() ==
                        "2972c92c7f534e37ee116ae01c707741");

                    if (ap->getName() == "uint8")
                    {
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 1);
                        TESTING_ASSERT(dims0[0] == 4);
                    }
                    else if (ap->getName() == "uint8_newDims")
                    {
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 2);
                        TESTING_ASSERT(dims0[0] == 2);
                        TESTING_ASSERT(dims0[1] == 2);
                    }
                }
                break;

                case Alembic::Util::kInt8POD:
                {
                    TESTING_ASSERT(ap->getName() == "int8");
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 2);
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 2);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Alembic::Util::int8_t * data =
                        (Alembic::Util::int8_t *)(val->getData());
                    TESTING_ASSERT(data[0] == -20);
                    TESTING_ASSERT(data[1] == 45);
                }
                break;

                case Alembic::Util::kUint16POD:
                {
                    TESTING_ASSERT(ap->getName() == "uint16");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 3);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 3);
                    Alembic::Util::uint16_t * data =
                        (Alembic::Util::uint16_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 60000);
                    TESTING_ASSERT(data[1] == 2);
                    TESTING_ASSERT(data[2] == 3987);
                }
                break;

                case Alembic::Util::kInt16POD:
                {
                    TESTING_ASSERT(ap->getName() == "int16");
                    TESTING_ASSERT(!ap->isScalarLike());
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 2);
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 2);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Alembic::Util::int16_t * data =
                        (Alembic::Util::int16_t *)(val->getData());
                    TESTING_ASSERT(data[0] == -20000);
                    TESTING_ASSERT(data[1] == 77);
                }
                break;

                case Alembic::Util::kUint32POD:
                {
                    TESTING_ASSERT(ap->getName() == "uint32");
                    TESTING_ASSERT(ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 1);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 1);
                    Alembic::Util::uint32_t * data =
                        (Alembic::Util::uint32_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 1000000);
                }
                break;

                case Alembic::Util::kInt32POD:
                {
                    TESTING_ASSERT(ap->getName() == "int32");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 4);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 4);
                    Alembic::Util::int32_t * data =
                        (Alembic::Util::int32_t *)(val->getData());
                    TESTING_ASSERT(data[0] == -1000000);
                    TESTING_ASSERT(data[1] == 42);
                    TESTING_ASSERT(data[2] == -20000);
                    TESTING_ASSERT(data[3] == 123456);
                }
                break;

                case Alembic::Util::kUint64POD:
                {
                    TESTING_ASSERT(ap->getName() == "uint64");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 3);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 3);
                    Alembic::Util::uint64_t * data =
                        (Alembic::Util::uint64_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 5000000000LL);
                    TESTING_ASSERT(data[1] == 1234567891011LL);
                    TESTING_ASSERT(data[2] == 12);
                }
                break;

                case Alembic::Util::kInt64POD:
                {
                    TESTING_ASSERT(ap->getName() == "int64");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 2);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 2);
                    Alembic::Util::int64_t * data =
                        (Alembic::Util::int64_t *)(val->getData());
                    TESTING_ASSERT(data[0] == -5000000000LL);
                    TESTING_ASSERT(data[1] == 9876543210LL);
                }
                break;

                case Alembic::Util::kFloat16POD:
                {
                    TESTING_ASSERT(ap->getName() == "float16");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().numPoints() == 2);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 2);
                    Alembic::Util::float16_t * data =
                        (Alembic::Util::float16_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 16.0);
                    TESTING_ASSERT(data[1] == -3.0);
                }
                break;

                case Alembic::Util::kFloat32POD:
                {
                    TESTING_ASSERT(ap->getName() == "float32" ||
                        ap->getName() == "float32_ext1");
                    TESTING_ASSERT(!ap->isScalarLike());
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);

                    if (ap->getName() == "float32_ext1")
                    {
                        TESTING_ASSERT(val->getDimensions().numPoints() == 4);
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 1);
                        TESTING_ASSERT(dims0.numPoints() == 4);
                    }

                    if (ap->getName() == "float32")
                    {
                        TESTING_ASSERT( ap->getDataType().getExtent() == 2);
                        TESTING_ASSERT(val->getDimensions().numPoints() == 2);
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 1);
                        TESTING_ASSERT(dims0.numPoints() == 2);
                    }

                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Alembic::Util::float32_t * data =
                        (Alembic::Util::float32_t *)(val->getData());
                    TESTING_ASSERT(data[0] == 128.0);
                    TESTING_ASSERT(data[1] == -13.25);
                    TESTING_ASSERT(data[2] == 35.5);
                    TESTING_ASSERT(data[3] == 128.125);
                }
                break;

                case Alembic::Util::kFloat64POD:
                {
                    TESTING_ASSERT(ap->getName() == "float64" ||
                        ap->getName() == "float64_ext3");
                    TESTING_ASSERT(!ap->isScalarLike());

                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);

                    if (ap->getName() == "float64")
                    {
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 1);
                        TESTING_ASSERT(dims0.numPoints() == 30);
                        TESTING_ASSERT(val->getDimensions().numPoints() == 30);
                    }

                    if (ap->getName() == "float64_ext3")
                    {
                        Dimensions dims0;
                        ap->getDimensions(0, dims0);
                        TESTING_ASSERT(dims0.rank() == 1);
                        TESTING_ASSERT(dims0.numPoints() == 10);
                        TESTING_ASSERT( ap->getDataType().getExtent() == 3);
                        TESTING_ASSERT(val->getDimensions().numPoints() == 10);
                    }

                    Alembic::Util::float64_t * data =
                        (Alembic::Util::float64_t *)(val->getData());
                    for (size_t i = 0; i < val->size(); ++i)
                        TESTING_ASSERT(data[i] == i* 100);
                }
                break;

                case Alembic::Util::kStringPOD:
                {
                    TESTING_ASSERT(ap->getName() == "str");
                    ABC::ArraySamplePtr val;
                    ap->getSample(0, val);
                    TESTING_ASSERT(!ap->isScalarLike());
                    TESTING_ASSERT(val->getDimensions().numPoints() == 4);
                    TESTING_ASSERT(val->getDimensions().rank() == 1);
                    Dimensions dims0;
                    ap->getDimensions(0, dims0);
                    TESTING_ASSERT(dims0.rank() == 1);
                    TESTING_ASSERT(dims0.numPoints() == 4);
                    Alembic::Util::string * data =
                        (Alembic::Util::string *)(val->getData());
                    TESTING_ASSERT(data[0] == "Now it's time");
                    TESTING_ASSERT(data[1] == "");
                    TESTING_ASSERT(data[2] == "for");
                    TESTING_ASSERT(data[3] == "cake!");
                }
                break;

            case Alembic::Util::kWstringPOD:
            {
                TESTING_ASSERT(ap->getName() == "wstr");
                ABC::ArraySamplePtr val;
                ap->getSample(0, val);
                TESTING_ASSERT(!ap->isScalarLike());
                TESTING_ASSERT(val->getDimensions().numPoints() == 4);
                TESTING_ASSERT(val->getDimensions().rank() == 1);
                Dimensions dims0;
                ap->getDimensions(0, dims0);
                TESTING_ASSERT(dims0.rank() == 1);
                TESTING_ASSERT(dims0.numPoints() == 4);
                Alembic::Util::wstring * data =
                    (Alembic::Util::wstring *)(val->getData());
                TESTING_ASSERT(data[0] == L"We do what we must ");
                TESTING_ASSERT(data[1] == L"because we can.");
                TESTING_ASSERT(data[2] == L"");
                TESTING_ASSERT(data[3] ==  L"\uf8e4 \uf8e2 \uf8d3");
            }
            break;

            default:
                TESTING_ASSERT(false);
                break;
            }
        }
    }  // end of reading
}

//-*****************************************************************************
void testEmptyArray()
{
    std::string archiveName = "emptyArray.abc";
    {
        A5::WriteArchive w;
        ABC::ArchiveWriterPtr a = w(archiveName, ABC::MetaData());
        ABC::ObjectWriterPtr archive = a->getTop();

        ABC::CompoundPropertyWriterPtr parent = archive->getProperties();

        ABC::ArrayPropertyWriterPtr awp =
            parent->createArrayProperty("emptyInt32", ABC::MetaData(),
                                        ABC::DataType(Alembic::Util::kInt32POD, 1), 0);
        TESTING_ASSERT(awp->getNumSamples() == 0);
    }

    {
        A5::ReadArchive r;
        ABC::ArchiveReaderPtr a = r( archiveName );
        ABC::ObjectReaderPtr archive = a->getTop();
        ABC::CompoundPropertyReaderPtr parent = archive->getProperties();

        TESTING_ASSERT(parent->getNumProperties() == 1);

        ABC::BasePropertyReaderPtr bp = parent->getProperty( 0 );
        TESTING_ASSERT(bp->isArray());
        TESTING_ASSERT(bp->asScalarPtr() == NULL);
        TESTING_ASSERT(bp->getParent() == parent);
        ABC::ArrayPropertyReaderPtr ap = bp->asArrayPtr();
        TESTING_ASSERT(ap->getNumSamples() == 0);
        TESTING_ASSERT(ap->getNumSamples() == 0);
        TESTING_ASSERT(ap->isConstant());
        ABC::ArraySamplePtr as;

        TESTING_ASSERT_THROW(ap->getSample(0, as), Alembic::Util::Exception);
    }

    {
        A5::WriteArchive w;
        ABC::ArchiveWriterPtr a = w(archiveName, ABC::MetaData());
        ABC::ObjectWriterPtr archive = a->getTop();

        ABC::CompoundPropertyWriterPtr parent = archive->getProperties();

        // no extent should throw
        TESTING_ASSERT_THROW(parent->createArrayProperty("emptyInt32",
                                                         ABC::MetaData(), ABC::DataType(Alembic::Util::kInt32POD, 0), 0),
                             Alembic::Util::Exception);

        // 2 parts of the enum that shouldn't be written out
        TESTING_ASSERT_THROW(parent->createArrayProperty("emptyInt32",
                                                         ABC::MetaData(), ABC::DataType(Alembic::Util::kUnknownPOD, 1), 0),
                             Alembic::Util::Exception);

        TESTING_ASSERT_THROW(parent->createArrayProperty("emptyInt32",
                                                         ABC::MetaData(),
                                                         ABC::DataType(Alembic::Util::kNumPlainOldDataTypes, 1), 0),
                             Alembic::Util::Exception);

        // illegal time sampling index
        TESTING_ASSERT_THROW(parent->createArrayProperty("emptyInt32",
                                                         ABC::MetaData(),
                                                         ABC::DataType(Alembic::Util::kInt32POD, 1), 45),
                             Alembic::Util::Exception);

        // lets do it again for the Scalars

        // no extent should throw
        TESTING_ASSERT_THROW(parent->createScalarProperty("emptyInt32",
                                                          ABC::MetaData(), ABC::DataType(Alembic::Util::kInt32POD, 0), 0),
                             Alembic::Util::Exception);

        // 2 parts of the enum that shouldn't be written out
        TESTING_ASSERT_THROW(parent->createScalarProperty("emptyInt32",
                                                          ABC::MetaData(),
                                                          ABC::DataType(Alembic::Util::kUnknownPOD, 1), 0),
                             Alembic::Util::Exception);

        TESTING_ASSERT_THROW(parent->createScalarProperty("emptyInt32",
                                                          ABC::MetaData(),
                                                          ABC::DataType(Alembic::Util::kNumPlainOldDataTypes, 1), 0),
                             Alembic::Util::Exception);

        // illegal time sampling index
        TESTING_ASSERT_THROW(parent->createScalarProperty("emptyInt32",
                                                          ABC::MetaData(),
                                                          ABC::DataType(Alembic::Util::kInt32POD, 1), 42),
                             Alembic::Util::Exception);

        parent->createArrayProperty("veryEmptyInt32",
                                    ABC::MetaData(), ABC::DataType(Alembic::Util::kInt32POD, 1), 0);

        // this should throw because the name isn't unique
        TESTING_ASSERT_THROW(parent->createScalarProperty("veryEmptyInt32",
                                                          ABC::MetaData(), ABC::DataType(Alembic::Util::kInt32POD, 1), 0),
                             Alembic::Util::Exception);

        ABC::ArrayPropertyWriterPtr ap = parent->createArrayProperty(
            "emptyInt64", ABC::MetaData(),
            ABC::DataType(Alembic::Util::kInt64POD, 1), 0);

        // In order to write a zero-length array, you still have to specify
        // non-rank-0 dimensions.
        ABC::Dimensions zeroLengthDims;
        zeroLengthDims.setRank( 1 );
        zeroLengthDims[0] = 0;
        ABC::ArraySample samp(NULL, ABC::DataType(Alembic::Util::kInt64POD, 1),
                              zeroLengthDims );
        ap->setSample( samp );
    }

    {
        A5::ReadArchive r;
        ABC::ArchiveReaderPtr a = r( archiveName );
        ABC::ObjectReaderPtr archive = a->getTop();
        ABC::CompoundPropertyReaderPtr parent = archive->getProperties();

        TESTING_ASSERT(parent->getNumProperties() == 2);

        for (size_t i = 0; i < parent->getNumProperties(); ++i)
        {
            ABC::BasePropertyReaderPtr bp = parent->getProperty( i );
            TESTING_ASSERT(bp->isArray());
            TESTING_ASSERT(bp->asScalarPtr() == NULL);
            TESTING_ASSERT(bp->getParent() == parent);
            ABC::ArrayPropertyReaderPtr ap = bp->asArrayPtr();
            if (ap->getName() == "veryEmptyInt32")
            {
                TESTING_ASSERT(ap->getNumSamples() == 0);

                // no samples?  it's scalar like
                TESTING_ASSERT(ap->isScalarLike());
            }
            else if (ap->getName() == "emptyInt64")
            {
                ABC::ArraySampleKey key;

                TESTING_ASSERT(ap->getNumSamples() == 1);
                ABC::ArraySamplePtr samp;
                ap->getSample(0, samp);
                Dimensions dims0;
                TESTING_ASSERT(dims0.numPoints() == 0);
                TESTING_ASSERT(samp->getDimensions().numPoints() == 0);
                TESTING_ASSERT(!ap->isScalarLike());
                TESTING_ASSERT(ap->getKey(0, key));
                TESTING_ASSERT(key.numBytes == 0);
                TESTING_ASSERT(key.origPOD == Alembic::Util::kInt64POD);
                TESTING_ASSERT(key.readPOD == Alembic::Util::kInt64POD);
                TESTING_ASSERT(key.digest.str() ==
                               "00000000000000000000000000000000");
            }
        }
    }
}

//-*****************************************************************************
void testExtentArrayStrings()
{
    std::string archiveName = "extentStrArray.abc";
    {
        A5::WriteArchive w;
        ABC::ArchiveWriterPtr a = w(archiveName, ABC::MetaData());
        ABC::ObjectWriterPtr archive = a->getTop();

        ABC::CompoundPropertyWriterPtr parent = archive->getProperties();

        ABC::DataType dtype(Alembic::Util::kStringPOD, 2);

        ABC::ArrayPropertyWriterPtr awp =
            parent->createArrayProperty("str", ABC::MetaData(), dtype, 0);

        std::vector < Alembic::Util::string > vals(6);
        vals[0] = "Peanut";
        vals[1] = "butter";
        vals[2] = "jelly";
        vals[3] = "time";
        vals[4] = "nom nom";
        vals[5] = "";

        std::vector < Alembic::Util::string > vals2(4);
        vals2[0] = "";
        vals2[1] = "Is the cake really a lie?";
        vals2[2] = "";
        vals2[3] = "I certainly hope not.";

        std::vector < Alembic::Util::string > vals3(4);
        vals3[0] = "Is the cake really a lie?";
        vals3[1] = "";
        vals3[2] = "";
        vals3[3] = "I certainly hope not.";

        awp->setSample(
            ABC::ArraySample(&(vals.front()), dtype,
                             Alembic::Util::Dimensions(3)));

        awp->setSample(
            ABC::ArraySample(&(vals2.front()), dtype,
                             Alembic::Util::Dimensions(2)));

        std::vector < Alembic::Util::string > badVals(1);
        badVals[0] = "This better fail.";
        TESTING_ASSERT_THROW(awp->setSample(
                                 ABC::ArraySample(&(badVals.front()),
                                                  ABC::DataType(Alembic::Util::kStringPOD, 1),
                                                  Alembic::Util::Dimensions(1))), Alembic::Util::Exception);

        awp->setSample(
            ABC::ArraySample(&(vals3.front()), dtype,
                             Alembic::Util::Dimensions(2)));

        awp->setSample(
            ABC::ArraySample(&(vals2.front()), dtype,
                             Alembic::Util::Dimensions(2)));

        std::vector < Alembic::Util::float32_t > vals5(3);
        vals5[0] = 1.0;
        vals5[1] = 2.0;
        vals5[2] = 3.0;

        // this should fail since we are trying to write float data to
        // a string array property
        TESTING_ASSERT_THROW(awp->setSample(
                                 ABC::ArraySample(&(vals5.front()),
                                                  ABC::DataType(Alembic::Util::kFloat32POD, 1),
                                                  Alembic::Util::Dimensions(3))), Alembic::Util::Exception);
    }

    {
        A5::ReadArchive r;
        ABC::ArchiveReaderPtr a = r( archiveName );
        ABC::ObjectReaderPtr archive = a->getTop();
        ABC::CompoundPropertyReaderPtr parent = archive->getProperties();
        TESTING_ASSERT(parent->getNumProperties() == 1);

        ABC::BasePropertyReaderPtr bp = parent->getProperty( 0 );
        TESTING_ASSERT(bp->isArray());
        ABC::ArrayPropertyReaderPtr ap = bp->asArrayPtr();
        TESTING_ASSERT(ap->getNumSamples() == 4);

        ABC::ArraySamplePtr val;

        ap->getSample(0, val);
        TESTING_ASSERT(val->getDimensions().numPoints() == 3);
        TESTING_ASSERT(val->getDimensions().rank() == 1);

        Dimensions dims0;
        ap->getDimensions(0, dims0);
        TESTING_ASSERT(dims0.numPoints() == 3);
        TESTING_ASSERT(dims0.rank() == 1);

        Alembic::Util::string * data = (Alembic::Util::string *)(val->getData());

        TESTING_ASSERT(data[0] == "Peanut");
        TESTING_ASSERT(data[1] == "butter");
        TESTING_ASSERT(data[2] == "jelly");
        TESTING_ASSERT(data[3] == "time");
        TESTING_ASSERT(data[4] == "nom nom");
        TESTING_ASSERT(data[5] == "");

        ABC::ArraySampleKey key;
        TESTING_ASSERT(ap->getKey(0, key));

        // includes NULL seperator
        TESTING_ASSERT(key.numBytes == 34);
        TESTING_ASSERT(key.origPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.readPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.digest.str() ==
                       "88d5ea1d772131992c9be9a192938df7");

        ap->getSample(1, val);
        TESTING_ASSERT(val->getDimensions().numPoints() == 2);
        TESTING_ASSERT(val->getDimensions().rank() == 1);

        ap->getDimensions(1, dims0);
        TESTING_ASSERT(dims0.numPoints() == 2);
        TESTING_ASSERT(dims0.rank() == 1);

        data = (Alembic::Util::string *)(val->getData());
        TESTING_ASSERT(data[0] == "");
        TESTING_ASSERT(data[1] == "Is the cake really a lie?");
        TESTING_ASSERT(data[2] == "");
        TESTING_ASSERT(data[3] == "I certainly hope not.");
        TESTING_ASSERT(ap->getKey(1, key));
        TESTING_ASSERT(key.numBytes == 50);
        TESTING_ASSERT(key.origPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.readPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.digest.str() ==
                       "126ebab40166a642d3c8cc4c5929074d");

        ap->getSample(2, val);
        TESTING_ASSERT(val->getDimensions().numPoints() == 2);
        TESTING_ASSERT(val->getDimensions().rank() == 1);
        ap->getDimensions(2, dims0);
        TESTING_ASSERT(dims0.numPoints() == 2);
        TESTING_ASSERT(dims0.rank() == 1);
        data = (Alembic::Util::string *)(val->getData());
        TESTING_ASSERT(data[0] == "Is the cake really a lie?");
        TESTING_ASSERT(data[1] == "");
        TESTING_ASSERT(data[2] == "");
        TESTING_ASSERT(data[3] == "I certainly hope not.");
        TESTING_ASSERT(ap->getKey(2, key));
        TESTING_ASSERT(key.numBytes == 50);
        TESTING_ASSERT(key.origPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.readPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.digest.str() ==
                       "a197a3747b0666dfacdc17634f5cabff");

        ap->getSample(3, val);
        ap->getDimensions(3, dims0);
        TESTING_ASSERT(dims0.numPoints() == 2);
        TESTING_ASSERT(dims0.rank() == 1);
        TESTING_ASSERT(val->getDimensions().numPoints() == 2);
        TESTING_ASSERT(val->getDimensions().rank() == 1);
        data = (Alembic::Util::string *)(val->getData());
        TESTING_ASSERT(data[0] == "");
        TESTING_ASSERT(data[1] == "Is the cake really a lie?");
        TESTING_ASSERT(data[2] == "");
        TESTING_ASSERT(data[3] == "I certainly hope not.");
        TESTING_ASSERT(ap->getKey(1, key));
        TESTING_ASSERT(key.numBytes == 50);
        TESTING_ASSERT(key.origPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.readPOD == Alembic::Util::kStringPOD);
        TESTING_ASSERT(key.digest.str() ==
                       "126ebab40166a642d3c8cc4c5929074d");
    }
}

int main ( int argc, char *argv[] )
{
    testEmptyArray();
    testDuplicateArray();
    testReadWriteArrays();
    testExtentArrayStrings();
    return 0;
}

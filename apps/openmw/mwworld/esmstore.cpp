#include "esmstore.hpp"

#include <set>
#include <iostream>

#include <boost/filesystem/operations.hpp>

#include <components/loadinglistener/loadinglistener.hpp>

#include <components/esm/esmreader.hpp>
#include <components/esm/esmwriter.hpp>

namespace MWWorld
{

static bool isCacheableRecord(int id)
{
    if (id == ESM::REC_ACTI || id == ESM::REC_ALCH || id == ESM::REC_APPA || id == ESM::REC_ARMO ||
        id == ESM::REC_BOOK || id == ESM::REC_CLOT || id == ESM::REC_CONT || id == ESM::REC_CREA ||
        id == ESM::REC_DOOR || id == ESM::REC_INGR || id == ESM::REC_LEVC || id == ESM::REC_LEVI ||
        id == ESM::REC_LIGH || id == ESM::REC_LOCK || id == ESM::REC_MISC || id == ESM::REC_NPC_ ||
        id == ESM::REC_PROB || id == ESM::REC_REPA || id == ESM::REC_STAT || id == ESM::REC_WEAP ||
        id == ESM::REC_BODY)
    {
        return true;
    }
    return false;
}

void ESMStore::load(ESM::ESMReader &esm, Loading::Listener* listener)
{
    listener->setProgressRange(1000);

    ESM::Dialogue *dialogue = 0;

    // Land texture loading needs to use a separate internal store for each plugin.
    // We set the number of plugins here to avoid continual resizes during loading,
    // and so we can properly verify if valid plugin indices are being passed to the
    // LandTexture Store retrieval methods.
    mLandTextures.resize(esm.getGlobalReaderList()->size());

    /// \todo Move this to somewhere else. ESMReader?
    // Cache parent esX files by tracking their indices in the global list of
    //  all files/readers used by the engine. This will greaty accelerate
    //  refnumber mangling, as required for handling moved references.
    const std::vector<ESM::Header::MasterData> &masters = esm.getGameFiles();
    std::vector<ESM::ESMReader> *allPlugins = esm.getGlobalReaderList();
    for (size_t j = 0; j < masters.size(); j++) {
        ESM::Header::MasterData &mast = const_cast<ESM::Header::MasterData&>(masters[j]);
        std::string fname = mast.name;
        int index = ~0;
        for (int i = 0; i < esm.getIndex(); i++) {
            const std::string &candidate = allPlugins->at(i).getContext().filename;
            std::string fnamecandidate = boost::filesystem::path(candidate).filename().string();
            if (Misc::StringUtils::ciEqual(fname, fnamecandidate)) {
                index = i;
                break;
            }
        }
        if (index == (int)~0) {
            // Tried to load a parent file that has not been loaded yet. This is bad,
            //  the launcher should have taken care of this.
            std::string fstring = "File " + esm.getName() + " asks for parent file " + masters[j].name
                + ", but it has not been loaded yet. Please check your load order.";
            esm.fail(fstring);
        }
        mast.index = index;
    }

    // Loop through all records
    while(esm.hasMoreRecs())
    {
        ESM::NAME n = esm.getRecName();
        esm.getRecHeader();

        // Look up the record type.
        std::map<int, StoreBase *>::iterator it = mStores.find(n.val);

        if (it == mStores.end()) {
            if (n.val == ESM::REC_INFO) {
                if (dialogue)
                {
                    dialogue->readInfo(esm, esm.getIndex() != 0);
                }
                else
                {
                    std::cerr << "error: info record without dialog" << std::endl;
                    esm.skipRecord();
                }
            } else if (n.val == ESM::REC_MGEF) {
                mMagicEffects.load (esm);
            } else if (n.val == ESM::REC_SKIL) {
                mSkills.load (esm);
            }
            else if (n.val==ESM::REC_FILT || n.val == ESM::REC_DBGP)
            {
                // ignore project file only records
                esm.skipRecord();
            }
            else {
                std::stringstream error;
                error << "Unknown record: " << n.toString();
                throw std::runtime_error(error.str());
            }
        } else {
            RecordId id = it->second->load(esm);
            if (id.mIsDeleted)
            {
                it->second->eraseStatic(id.mId);
                continue;
            }

            if (n.val==ESM::REC_DIAL) {
                dialogue = const_cast<ESM::Dialogue*>(mDialogs.find(id.mId));
            } else {
                dialogue = 0;
            }
        }
        listener->setProgress(static_cast<size_t>(esm.getFileOffset() / (float)esm.getFileSize() * 1000));
    }
}

void ESMStore::setUp()
{
    mIds.clear();

    std::map<int, StoreBase *>::iterator storeIt = mStores.begin();
    for (; storeIt != mStores.end(); ++storeIt) {
        storeIt->second->setUp();

        if (isCacheableRecord(storeIt->first))
        {
            std::vector<std::string> identifiers;
            storeIt->second->listIdentifier(identifiers);

            for (std::vector<std::string>::const_iterator record = identifiers.begin(); record != identifiers.end(); ++record)
                mIds[*record] = storeIt->first;
        }
    }
    mSkills.setUp();
    mMagicEffects.setUp();
    mAttributes.setUp();
    mDialogs.setUp();
}

    int ESMStore::countSavedGameRecords() const
    {
        return 1 // DYNA (dynamic name counter)
            +mPotions.getDynamicSize()
            +mArmors.getDynamicSize()
            +mBooks.getDynamicSize()
            +mClasses.getDynamicSize()
            +mClothes.getDynamicSize()
            +mEnchants.getDynamicSize()
            +mNpcs.getDynamicSize()
            +mSpells.getDynamicSize()
            +mWeapons.getDynamicSize()
            +mCreatureLists.getDynamicSize()
            +mItemLists.getDynamicSize();
    }

    void ESMStore::write (ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        writer.startRecord(ESM::REC_DYNA);
        writer.startSubRecord("COUN");
        writer.writeT(mDynamicCount);
        writer.endRecord("COUN");
        writer.endRecord(ESM::REC_DYNA);

        mPotions.write (writer, progress);
        mArmors.write (writer, progress);
        mBooks.write (writer, progress);
        mClasses.write (writer, progress);
        mClothes.write (writer, progress);
        mEnchants.write (writer, progress);
        mSpells.write (writer, progress);
        mWeapons.write (writer, progress);
        mNpcs.write (writer, progress);
        mItemLists.write (writer, progress);
        mCreatureLists.write (writer, progress);
    }

    bool ESMStore::readRecord (ESM::ESMReader& reader, uint32_t type)
    {
        switch (type)
        {
            case ESM::REC_ALCH:
            case ESM::REC_ARMO:
            case ESM::REC_BOOK:
            case ESM::REC_CLAS:
            case ESM::REC_CLOT:
            case ESM::REC_ENCH:
            case ESM::REC_SPEL:
            case ESM::REC_WEAP:
            case ESM::REC_NPC_:
            case ESM::REC_LEVI:
            case ESM::REC_LEVC:

                {
                    mStores[type]->read (reader);
                }

                if (type==ESM::REC_NPC_)
                {
                    // NPC record will always be last and we know that there can be only one
                    // dynamic NPC record (player) -> We are done here with dynamic record loading
                    setUp();

                    const ESM::NPC *player = mNpcs.find ("player");

                    if (!mRaces.find (player->mRace) ||
                        !mClasses.find (player->mClass))
                        throw std::runtime_error ("Invalid player record (race or class unavailable");
                }

                return true;

            case ESM::REC_DYNA:
                reader.getSubNameIs("COUN");
                reader.getHT(mDynamicCount);
                return true;

            default:

                return false;
        }
    }

} // end namespace

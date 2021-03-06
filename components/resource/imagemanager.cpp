#include "imagemanager.hpp"

#include <osgDB/Registry>
#include <osg/GLExtensions>
#include <osg/Version>

#include <components/vfs/manager.hpp>

#include "objectcache.hpp"

#ifdef OSG_LIBRARY_STATIC
// This list of plugins should match with the list in the top-level CMakelists.txt.
USE_OSGPLUGIN(png)
USE_OSGPLUGIN(tga)
USE_OSGPLUGIN(dds)
USE_OSGPLUGIN(jpeg)
#endif

namespace
{

    osg::ref_ptr<osg::Image> createWarningImage()
    {
        osg::ref_ptr<osg::Image> warningImage = new osg::Image;

        int width = 8, height = 8;
        warningImage->allocateImage(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE);
        assert (warningImage->isDataContiguous());
        unsigned char* data = warningImage->data();
        for (int i=0;i<width*height;++i)
        {
            data[3*i] = (255);
            data[3*i+1] = (0);
            data[3*i+2] = (255);
        }
        return warningImage;
    }

}

namespace Resource
{

    ImageManager::ImageManager(const VFS::Manager *vfs)
        : ResourceManager(vfs)
        , mWarningImage(createWarningImage())
        , mOptions(new osgDB::Options("dds_flip dds_dxt1_detect_rgba"))
    {
    }

    ImageManager::~ImageManager()
    {

    }

    bool checkSupported(osg::Image* image, const std::string& filename)
    {
        switch(image->getPixelFormat())
        {
            case(GL_COMPRESSED_RGB_S3TC_DXT1_EXT):
            case(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT):
            case(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT):
            case(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT):
            {
#if OSG_VERSION_GREATER_OR_EQUAL(3,3,3)
                osg::GLExtensions* exts = osg::GLExtensions::Get(0, false);
                if (exts && !exts->isTextureCompressionS3TCSupported
                        // This one works too. Should it be included in isTextureCompressionS3TCSupported()? Submitted as a patch to OSG.
                        && !osg::isGLExtensionSupported(0, "GL_S3_s3tc"))
#else
                osg::Texture::Extensions* exts = osg::Texture::getExtensions(0, false);
                if (exts && !exts->isTextureCompressionS3TCSupported()
                        // This one works too. Should it be included in isTextureCompressionS3TCSupported()? Submitted as a patch to OSG.
                        && !osg::isGLExtensionSupported(0, "GL_S3_s3tc"))
#endif
                {
                    std::cerr << "Error loading " << filename << ": no S3TC texture compression support installed" << std::endl;
                    return false;
                }
                break;
            }
            // not bothering with checks for other compression formats right now, we are unlikely to ever use those anyway
            default:
                return true;
        }
        return true;
    }

    osg::ref_ptr<osg::Image> ImageManager::getImage(const std::string &filename)
    {
        std::string normalized = filename;
        mVFS->normalizeFilename(normalized);

        osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(normalized);
        if (obj)
            return osg::ref_ptr<osg::Image>(static_cast<osg::Image*>(obj.get()));
        else
        {
            Files::IStreamPtr stream;
            try
            {
                stream = mVFS->get(normalized.c_str());
            }
            catch (std::exception& e)
            {
                std::cerr << "Failed to open image: " << e.what() << std::endl;
                mCache->addEntryToObjectCache(normalized, mWarningImage);
                return mWarningImage;
            }

            size_t extPos = normalized.find_last_of('.');
            std::string ext;
            if (extPos != std::string::npos && extPos+1 < normalized.size())
                ext = normalized.substr(extPos+1);
            osgDB::ReaderWriter* reader = osgDB::Registry::instance()->getReaderWriterForExtension(ext);
            if (!reader)
            {
                std::cerr << "Error loading " << filename << ": no readerwriter for '" << ext << "' found" << std::endl;
                mCache->addEntryToObjectCache(normalized, mWarningImage);
                return mWarningImage;
            }

            osgDB::ReaderWriter::ReadResult result = reader->readImage(*stream, mOptions);
            if (!result.success())
            {
                std::cerr << "Error loading " << filename << ": " << result.message() << " code " << result.status() << std::endl;
                mCache->addEntryToObjectCache(normalized, mWarningImage);
                return mWarningImage;
            }

            osg::Image* image = result.getImage();
            if (!checkSupported(image, filename))
            {
                mCache->addEntryToObjectCache(normalized, mWarningImage);
                return mWarningImage;
            }

            mCache->addEntryToObjectCache(normalized, image);
            return image;
        }
    }

    osg::Image *ImageManager::getWarningImage()
    {
        return mWarningImage;
    }

}

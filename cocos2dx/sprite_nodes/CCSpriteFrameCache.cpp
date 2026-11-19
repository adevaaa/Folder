/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2009      Jason Booth
Copyright (c) 2009      Robert J Payne
Copyright (c) 2011      Zynga Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "cocoa/CCNS.h"
#include "ccMacros.h"
#include "textures/CCTextureCache.h"
#include "CCSpriteFrameCache.h"
#include "CCSpriteFrame.h"
#include "CCSprite.h"
#include "support/TransformUtils.h"
#include "platform/CCFileUtils.h"
#include "cocoa/CCString.h"
#include "cocoa/CCArray.h"
#include "cocoa/CCDictionary.h"
#include <vector>
#include <cstdio>
#include <cstring>

using namespace std;

NS_CC_BEGIN

static CCSpriteFrameCache *pSharedSpriteFrameCache = NULL;

CCSpriteFrameCache *CCSpriteFrameCache::sharedSpriteFrameCache(void)
{
    if (!pSharedSpriteFrameCache)
    {
        pSharedSpriteFrameCache = new CCSpriteFrameCache();
        pSharedSpriteFrameCache->init();
    }

    return pSharedSpriteFrameCache;
}

void CCSpriteFrameCache::purgeSharedSpriteFrameCache(void)
{
    CC_SAFE_RELEASE_NULL(pSharedSpriteFrameCache);
}

bool CCSpriteFrameCache::init(void)
{
    m_pSpriteFrames = new CCDictionary();
    m_pSpriteFramesAliases = new CCDictionary();
    m_pLoadedFileNames = new std::set<std::string>();
    m_pLooseFrames = new CCDictionary();
    m_pLooseMissing = new std::set<std::string>();
    m_pLooseMissingAny = new std::set<std::string>();
    m_pLoosePaths = new std::vector<std::string>();
    return true;
}

CCSpriteFrameCache::~CCSpriteFrameCache(void)
{
    CC_SAFE_RELEASE(m_pSpriteFrames);
    CC_SAFE_RELEASE(m_pSpriteFramesAliases);
    CC_SAFE_DELETE(m_pLoadedFileNames);
    CC_SAFE_RELEASE(m_pLooseFrames);
    CC_SAFE_DELETE(m_pLooseMissing);
    CC_SAFE_DELETE(m_pLooseMissingAny);
    CC_SAFE_DELETE(m_pLoosePaths);
}

void CCSpriteFrameCache::addSpriteFramesWithDictionary(CCDictionary *dictionary, CCTexture2D *pobTexture)
{
    /*
    Supported Zwoptex Formats:

    ZWTCoordinatesFormatOptionXMLLegacy = 0, // Flash Version
    ZWTCoordinatesFormatOptionXML1_0 = 1, // Desktop Version 0.0 - 0.4b
    ZWTCoordinatesFormatOptionXML1_1 = 2, // Desktop Version 1.0.0 - 1.0.1
    ZWTCoordinatesFormatOptionXML1_2 = 3, // Desktop Version 1.0.2+
    */

    CCDictionary *metadataDict = (CCDictionary *)dictionary->objectForKey("metadata");
    CCDictionary *framesDict = (CCDictionary *)dictionary->objectForKey("frames");
    int format = 0;

    // get the format
    if (metadataDict != NULL)
    {
        format = metadataDict->valueForKey("format")->intValue();
    }

    // check the format
    CCAssert(format >= 0 && format <= 3, "format is not supported for CCSpriteFrameCache addSpriteFramesWithDictionary:textureFilename:");

    CCDictElement *pElement = NULL;
    CCDICT_FOREACH(framesDict, pElement)
    {
        CCDictionary *frameDict = (CCDictionary *)pElement->getObject();
        std::string spriteFrameName = pElement->getStrKey();
        CCSpriteFrame *spriteFrame = (CCSpriteFrame *)m_pSpriteFrames->objectForKey(spriteFrameName);
        if (spriteFrame)
        {
            continue;
        }

        if (format == 0)
        {
            float x = frameDict->valueForKey("x")->floatValue();
            float y = frameDict->valueForKey("y")->floatValue();
            float w = frameDict->valueForKey("width")->floatValue();
            float h = frameDict->valueForKey("height")->floatValue();
            float ox = frameDict->valueForKey("offsetX")->floatValue();
            float oy = frameDict->valueForKey("offsetY")->floatValue();
            int ow = frameDict->valueForKey("originalWidth")->intValue();
            int oh = frameDict->valueForKey("originalHeight")->intValue();
            // check ow/oh
            if (!ow || !oh)
            {
                CCLOGWARN("cocos2d: WARNING: originalWidth/Height not found on the CCSpriteFrame. AnchorPoint won't work as expected. Regenrate the .plist");
            }
            // abs ow/oh
            ow = abs(ow);
            oh = abs(oh);
            // create frame
            spriteFrame = new CCSpriteFrame();
            spriteFrame->initWithTexture(pobTexture,
                                         CCRectMake(x, y, w, h),
                                         false,
                                         CCPointMake(ox, oy),
                                         CCSizeMake((float)ow, (float)oh));
        }
        else if (format == 1 || format == 2)
        {
            CCRect frame = CCRectFromString(frameDict->valueForKey("frame")->getCString());
            bool rotated = false;

            // rotation
            if (format == 2)
            {
                rotated = frameDict->valueForKey("rotated")->boolValue();
            }

            CCPoint offset = CCPointFromString(frameDict->valueForKey("offset")->getCString());
            CCSize sourceSize = CCSizeFromString(frameDict->valueForKey("sourceSize")->getCString());

            // create frame
            spriteFrame = new CCSpriteFrame();
            spriteFrame->initWithTexture(pobTexture,
                                         frame,
                                         rotated,
                                         offset,
                                         sourceSize);
        }
        else if (format == 3)
        {
            // get values
            // NOTE: Added anchor node by Re
            /** The plist file must have anchor property (Enable sprite pivot points on TextuerPacker)
             * Use texture packer will get some anchor errors
             * For v3 format we should modify the source code
             */
            CCSize spriteSize = CCSizeFromString(frameDict->valueForKey("spriteSize")->getCString());
            CCPoint spriteOffset = CCPointFromString(frameDict->valueForKey("spriteOffset")->getCString());
            CCSize spriteSourceSize = CCSizeFromString(frameDict->valueForKey("spriteSourceSize")->getCString());
            CCRect textureRect = CCRectFromString(frameDict->valueForKey("textureRect")->getCString());
            bool textureRotated = frameDict->valueForKey("textureRotated")->boolValue();

			auto anchor = frameDict->valueForKey("anchor")->getCString();
			if (anchor)
			{
				CCPoint anchorPoint = CCPointFromString(anchor);
				spriteOffset.x = (anchorPoint.x - 0.5f) * 100;
				spriteOffset.y = (anchorPoint.y - 0.5f) * 100;
			}

            // get aliases
            CCArray *aliases = (CCArray *)(frameDict->objectForKey("aliases"));
            CCString *frameKey = new CCString(spriteFrameName);

            CCObject *pObj = NULL;
            CCARRAY_FOREACH(aliases, pObj)
            {
                std::string oneAlias = ((CCString *)pObj)->getCString();
                if (m_pSpriteFramesAliases->objectForKey(oneAlias.c_str()))
                {
                    CCLOGWARN("cocos2d: WARNING: an alias with name %s already exists", oneAlias.c_str());
                }

                m_pSpriteFramesAliases->setObject(frameKey, oneAlias.c_str());
            }
            frameKey->release();
            // create frame
            spriteFrame = new CCSpriteFrame();
            spriteFrame->initWithTexture(pobTexture,
                                         CCRectMake(textureRect.origin.x, textureRect.origin.y, spriteSize.width, spriteSize.height),
                                         textureRotated,
                                         spriteOffset,
                                         spriteSourceSize);
        }

        // add sprite frame
        m_pSpriteFrames->setObject(spriteFrame, spriteFrameName);
        spriteFrame->release();
    }
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char *pszPlist, CCTexture2D *pobTexture)
{
    if (m_pLoadedFileNames->find(pszPlist) != m_pLoadedFileNames->end())
    {
        return; //We already added it
    }
    std::string fullPath = CCFileUtils::sharedFileUtils()->fullPathForFilename(pszPlist);
    CCDictionary *dict = CCDictionary::createWithContentsOfFileThreadSafe(fullPath.c_str());

    addSpriteFramesWithDictionary(dict, pobTexture);
    m_pLoadedFileNames->insert(pszPlist);

    dict->release();
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char *plist, const char *textureFileName)
{
    CCAssert(textureFileName, "texture name should not be null");
    CCTexture2D *texture = CCTextureCache::sharedTextureCache()->addImage(textureFileName);

    if (texture)
    {
        addSpriteFramesWithFile(plist, texture);
    }
    else
    {
        CCLOG("cocos2d: CCSpriteFrameCache: couldn't load texture file. File not found %s", textureFileName);
    }
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char *pszPlist)
{
    CCAssert(pszPlist, "plist filename should not be NULL");

    if (m_pLoadedFileNames->find(pszPlist) == m_pLoadedFileNames->end())
    {
        std::string fullPath = CCFileUtils::sharedFileUtils()->fullPathForFilename(pszPlist);
        CCDictionary *dict = CCDictionary::createWithContentsOfFileThreadSafe(fullPath.c_str());

        string texturePath("");

        CCDictionary *metadataDict = (CCDictionary *)dict->objectForKey("metadata");
        if (metadataDict)
        {
            // try to read  texture file name from meta data
            texturePath = metadataDict->valueForKey("textureFileName")->getCString();
        }

        if (!texturePath.empty())
        {
            // build texture path relative to plist file
            texturePath = CCFileUtils::sharedFileUtils()->fullPathFromRelativeFile(texturePath.c_str(), pszPlist);
        }
        else
        {
            // build texture path by replacing file extension
            texturePath = pszPlist;

            // remove .xxx
            size_t startPos = texturePath.find_last_of(".");
            texturePath = texturePath.erase(startPos);

            // append .png
            texturePath = texturePath.append(".png");

            CCLOG("cocos2d: CCSpriteFrameCache: Trying to use file %s as texture", texturePath.c_str());
        }

        CCTexture2D *pTexture = CCTextureCache::sharedTextureCache()->addImage(texturePath.c_str());

        if (pTexture)
        {
            addSpriteFramesWithDictionary(dict, pTexture);
            m_pLoadedFileNames->insert(pszPlist);
        }
        else
        {
            CCLOG("cocos2d: CCSpriteFrameCache: Couldn't load texture");
        }

        dict->release();
    }
}

void CCSpriteFrameCache::addSpriteFrame(CCSpriteFrame *pobFrame, const char *pszFrameName)
{
    m_pSpriteFrames->setObject(pobFrame, pszFrameName);
}

void CCSpriteFrameCache::removeSpriteFrames(void)
{
    m_pSpriteFrames->removeAllObjects();
    m_pSpriteFramesAliases->removeAllObjects();
    m_pLoadedFileNames->clear();
}

void CCSpriteFrameCache::removeUnusedSpriteFrames(void)
{
    bool bRemoved = false;
    CCDictElement *pElement = NULL;
    CCDICT_FOREACH(m_pSpriteFrames, pElement)
    {
        CCSpriteFrame *spriteFrame = (CCSpriteFrame *)pElement->getObject();
        if (spriteFrame->retainCount() == 1)
        {
            CCLOG("cocos2d: CCSpriteFrameCache: removing unused frame: %s", pElement->getStrKey());
            m_pSpriteFrames->removeObjectForElememt(pElement);
            bRemoved = true;
        }
    }

    // XXX. Since we don't know the .plist file that originated the frame, we must remove all .plist from the cache
    if (bRemoved)
    {
        m_pLoadedFileNames->clear();
    }
}

void CCSpriteFrameCache::removeSpriteFrameByName(const char *pszName)
{
    // explicit nil handling
    if (!pszName)
    {
        return;
    }

    // Is this an alias ?
    CCString *key = (CCString *)m_pSpriteFramesAliases->objectForKey(pszName);

    if (key)
    {
        m_pSpriteFrames->removeObjectForKey(key->getCString());
        m_pSpriteFramesAliases->removeObjectForKey(key->getCString());
    }
    else
    {
        m_pSpriteFrames->removeObjectForKey(pszName);
    }

    // XXX. Since we don't know the .plist file that originated the frame, we must remove all .plist from the cache
    m_pLoadedFileNames->clear();
}

void CCSpriteFrameCache::removeSpriteFramesFromFile(const char *plist)
{
    std::string fullPath = CCFileUtils::sharedFileUtils()->fullPathForFilename(plist);
    CCDictionary *dict = CCDictionary::createWithContentsOfFileThreadSafe(fullPath.c_str());

    removeSpriteFramesFromDictionary((CCDictionary *)dict);

    // remove it from the cache
    set<string>::iterator ret = m_pLoadedFileNames->find(plist);
    if (ret != m_pLoadedFileNames->end())
    {
        m_pLoadedFileNames->erase(ret);
    }

    dict->release();
}

void CCSpriteFrameCache::removeSpriteFramesFromDictionary(CCDictionary *dictionary)
{
    CCDictionary *framesDict = (CCDictionary *)dictionary->objectForKey("frames");
    CCArray *keysToRemove = CCArray::create();

    CCDictElement *pElement = NULL;
    CCDICT_FOREACH(framesDict, pElement)
    {
        if (m_pSpriteFrames->objectForKey(pElement->getStrKey()))
        {
            keysToRemove->addObject(CCString::create(pElement->getStrKey()));
        }
    }

    m_pSpriteFrames->removeObjectsForKeys(keysToRemove);
}

void CCSpriteFrameCache::removeSpriteFramesFromTexture(CCTexture2D *texture)
{
    CCArray *keysToRemove = CCArray::create();

    CCDictElement *pElement = NULL;
    CCDICT_FOREACH(m_pSpriteFrames, pElement)
    {
        string key = pElement->getStrKey();
        CCSpriteFrame *frame = (CCSpriteFrame *)m_pSpriteFrames->objectForKey(key.c_str());
        if (frame && (frame->getTexture() == texture))
        {
            keysToRemove->addObject(CCString::create(pElement->getStrKey()));
        }
    }

    m_pSpriteFrames->removeObjectsForKeys(keysToRemove);
}

// ---- [Aris] PNG lepas ---------------------------------------------------------------------------

static const char* kLooseAllFolders[] = {"UI", "Select", "Result", "Report", "Record", "Ougis",
                                         "Menu", "Gears", "Map", "NamePlate", "Ranking"};

static bool looseIsDigits(const char* s, size_t n)
{
    if (n == 0) return false;
    for (size_t i = 0; i < n; ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

// Folder kategori dari akhiran nama frame, tanpa alokasi. NULL jika akhirannya tidak dikenal.
static const char* looseFolderForName(const char* name)
{
    const size_t len = strlen(name);
    if (len < 6 || strcmp(name + len - 4, ".png") != 0)
        return NULL;
    const size_t end = len - 4;

    // token terakhir setelah '_' terakhir
    size_t us = end;
    while (us > 0 && name[us - 1] != '_') --us;
    if (us == 0) return NULL;               // tidak ada '_'
    const char* tok = name + us;
    size_t tlen = end - us;
    if (tlen == 0 || tlen > 12) return NULL;

    // _skill1, _skill1_1 (token terakhir hanya angka -> lihat token sebelumnya)
    if (looseIsDigits(tok, tlen) && us >= 2)
    {
        size_t us2 = us - 1;                  // posisi '_' sebelum token terakhir
        size_t b = us2;
        while (b > 0 && name[b - 1] != '_') --b;
        if (b > 0 && us2 - b >= 6 && strncmp(name + b, "skill", 5) == 0 && looseIsDigits(name + b + 5, us2 - b - 5))
            return "UI";
    }
    if (tlen > 5 && strncmp(tok, "skill", 5) == 0 && looseIsDigits(tok + 5, tlen - 5))
        return "UI";

    // buang angka di belakang akhiran (half2, full3, select2)
    while (tlen > 0 && tok[tlen - 1] >= '0' && tok[tlen - 1] <= '9') --tlen;
    if (tlen == 0) return NULL;

    struct Entry { const char* suffix; const char* folder; };
    static const Entry kEntries[] = {
        {"avator", "UI"},     {"font", "Select"},  {"half", "Select"}, {"select", "Select"},
        {"small", "Result"},  {"rp", "Report"},    {"rpf", "Report"},  {"full", "Record"},
        {"CutIn", "Ougis"}};
    for (size_t i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); ++i)
        if (strlen(kEntries[i].suffix) == tlen && strncmp(tok, kEntries[i].suffix, tlen) == 0)
            return kEntries[i].folder;
    return NULL;
}

void CCSpriteFrameCache::setLooseRoot(const char* root)
{
    m_looseRoot = root ? root : "";
    if (!m_looseRoot.empty() && m_looseRoot[m_looseRoot.size() - 1] != '/')
        m_looseRoot += '/';
    purgeLooseFrames();
}

void CCSpriteFrameCache::purgeLooseFrames(void)
{
    if (!m_pLooseFrames) return;
    m_pLooseFrames->removeAllObjects();
    m_pLooseMissing->clear();
    m_pLooseMissingAny->clear();
    for (size_t i = 0; i < m_pLoosePaths->size(); ++i)
        CCTextureCache::sharedTextureCache()->removeTextureForKey((*m_pLoosePaths)[i].c_str());
    m_pLoosePaths->clear();
}

CCSpriteFrame* CCSpriteFrameCache::findLooseFrame(const char* pszName, const char** folders, int folderCount, std::set<std::string>* missing)
{
    CCSpriteFrame* cached = (CCSpriteFrame*)m_pLooseFrames->objectForKey(pszName);
    if (cached) return cached;
    if (missing->find(pszName) != missing->end()) return NULL;

    for (int i = 0; i < folderCount; ++i)
    {
        std::string path = m_looseRoot + folders[i] + "/" + pszName;
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) continue;
        fclose(fp);

        CCTexture2D* texture = CCTextureCache::sharedTextureCache()->addImage(path.c_str());
        if (!texture)
        {
            CCLOG("[Aris] PNG tidak bisa dimuat: %s", path.c_str());
            continue;
        }
        const CCSize size = texture->getContentSizeInPixels();
        CCSpriteFrame* frame = CCSpriteFrame::createWithTexture(texture, CCRectMake(0, 0, size.width, size.height));
        if (!frame) continue;
        m_pLooseFrames->setObject(frame, pszName);
        m_pLoosePaths->push_back(path);
        CCLOG("[Aris] frame dari PNG lepas: %s", path.c_str());
        return frame;
    }
    missing->insert(pszName);
    return NULL;
}

CCSpriteFrame *CCSpriteFrameCache::spriteFrameByName(const char *pszName)
{
    const bool loose = !m_looseRoot.empty() && pszName != NULL;
    if (loose)
    {
        // 1. PNG lepas di folder kategori menimpa frame atlas bernama sama
        const char* folder = looseFolderForName(pszName);
        if (folder)
        {
            CCSpriteFrame* lf = findLooseFrame(pszName, &folder, 1, m_pLooseMissing);
            if (lf) return lf;
        }
    }

    CCSpriteFrame *frame = (CCSpriteFrame *)m_pSpriteFrames->objectForKey(pszName);
    if (!frame)
    {
        // try alias dictionary
        CCString *key = (CCString *)m_pSpriteFramesAliases->objectForKey(pszName);
        if (key)
        {
            frame = (CCSpriteFrame *)m_pSpriteFrames->objectForKey(key->getCString());
            if (!frame)
            {
                CCLOG("cocos2d: CCSpriteFrameCache: Frame '%s' not found", pszName);
            }
        }
    }

    // 2. tidak ada di atlas: cari di semua folder kategori (karakter baru tanpa atlas)
    if (!frame && loose)
    {
        const size_t len = strlen(pszName);
        if (len > 4 && strcmp(pszName + len - 4, ".png") == 0)
            frame = findLooseFrame(pszName, kLooseAllFolders, (int)(sizeof(kLooseAllFolders) / sizeof(kLooseAllFolders[0])), m_pLooseMissingAny);
    }
    return frame;
}

NS_CC_END

//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef GUI_ASSET_H
#include "GUIAsset.h"
#endif

#ifndef _ASSET_MANAGER_H_
#include "assets/assetManager.h"
#endif

#ifndef _CONSOLETYPES_H_
#include "console/consoleTypes.h"
#endif

#ifndef _TAML_
#include "persistence/taml/taml.h"
#endif

#ifndef _ASSET_PTR_H_
#include "assets/assetPtr.h"
#endif

// Debug Profiling.
#include "platform/profiler.h"

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GUIAsset);

ConsoleType(GUIAssetPtr, TypeGUIAssetPtr, const char*, ASSET_ID_FIELD_PREFIX)

//-----------------------------------------------------------------------------

ConsoleGetType(TypeGUIAssetPtr)
{
   // Fetch asset Id.
   return *((const char**)(dptr));
}

//-----------------------------------------------------------------------------

ConsoleSetType(TypeGUIAssetPtr)
{
   // Was a single argument specified?
   if (argc == 1)
   {
      // Yes, so fetch field value.
      *((const char**)dptr) = StringTable->insert(argv[0]);

      return;
   }

   // Warn.
   Con::warnf("(TypeGUIAssetPtr) - Cannot set multiple args to a single asset.");
}

//-----------------------------------------------------------------------------

GUIAsset::GUIAsset()
{
   mScriptFile = StringTable->EmptyString();
   mGUIFile = StringTable->EmptyString();

   mScriptPath = StringTable->EmptyString();
   mGUIPath = StringTable->EmptyString();
}

//-----------------------------------------------------------------------------

GUIAsset::~GUIAsset()
{
}

//-----------------------------------------------------------------------------

void GUIAsset::initPersistFields()
{
   // Call parent.
   Parent::initPersistFields();

   addProtectedField("scriptFile", TypeAssetLooseFilePath, Offset(mScriptFile, GUIAsset),
      &setScriptFile, &getScriptFile, "Path to the script file for the gui");

   addProtectedField("GUIFile", TypeAssetLooseFilePath, Offset(mGUIFile, GUIAsset),
      &setScriptFile, &getScriptFile, "Path to the gui file");
}

//------------------------------------------------------------------------------

void GUIAsset::copyTo(SimObject* object)
{
   // Call to parent.
   Parent::copyTo(object);
}

void GUIAsset::initializeAsset()
{
   mGUIPath = getOwned() ? expandAssetFilePath(mGUIFile) : mGUIPath;

   if (Torque::FS::IsScriptFile(mGUIPath))
      Con::executeFile(mGUIPath, false, false);

   mScriptPath = getOwned() ? expandAssetFilePath(mScriptFile) : mScriptPath;

   if (Torque::FS::IsScriptFile(mScriptPath))
      Con::executeFile(mScriptPath, false, false);
}

void GUIAsset::onAssetRefresh()
{
   mGUIPath = getOwned() ? expandAssetFilePath(mGUIFile) : mGUIPath;

   if (Torque::FS::IsScriptFile(mGUIPath))
      Con::executeFile(mGUIPath, false, false);

   mScriptPath = getOwned() ? expandAssetFilePath(mScriptFile) : mScriptPath;

   if (Torque::FS::IsScriptFile(mScriptPath))
      Con::executeFile(mScriptPath, false, false);
}

void GUIAsset::setGUIFile(const char* pScriptFile)
{
   // Sanity!
   AssertFatal(pScriptFile != NULL, "Cannot use a NULL gui file.");

   // Fetch image file.
   pScriptFile = StringTable->insert(pScriptFile, true);

   // Ignore no change,
   if (pScriptFile == mGUIFile)
      return;

   // Update.
   mGUIFile = getOwned() ? expandAssetFilePath(pScriptFile) : pScriptFile;

   // Refresh the asset.
   refreshAsset();
}

void GUIAsset::setScriptFile(const char* pScriptFile)
{
   // Sanity!
   AssertFatal(pScriptFile != NULL, "Cannot use a NULL script file.");

   // Fetch image file.
   pScriptFile = StringTable->insert(pScriptFile, true);

   // Ignore no change,
   if (pScriptFile == mScriptFile)
      return;

   // Update.
   mScriptFile = getOwned() ? expandAssetFilePath(pScriptFile) : pScriptFile;

   // Refresh the asset.
   refreshAsset();
}

StringTableEntry GUIAsset::getAssetIdByGUIName(StringTableEntry guiName)
{
   StringTableEntry assetId = StringTable->EmptyString();

   AssetQuery* query = new AssetQuery();
   U32 foundCount = AssetDatabase.findAssetType(query, "GUIAsset");
   if (foundCount == 0)
   {
      //Didn't work, so have us fall back to a placeholder asset
      assetId = StringTable->insert("Core_Rendering:noMaterial");
   }
   else
   {
      GuiControl* guiObject;
      if (!Sim::findObject(guiName, guiObject))
         return "";

      StringTableEntry guiFile = guiObject->getFilename();

      for (U32 i = 0; i < foundCount; i++)
      {
         GUIAsset* guiAsset = AssetDatabase.acquireAsset<GUIAsset>(query->mAssetList[i]);
         if (guiAsset && guiAsset->getGUIPath() == guiFile)
         {
            assetId = guiAsset->getAssetId();
            AssetDatabase.releaseAsset(query->mAssetList[i]);
            break;
         }
         AssetDatabase.releaseAsset(query->mAssetList[i]);
      }
   }

   return assetId;
}

#ifdef TORQUE_TOOLS
DefineEngineStaticMethod(GUIAsset, getAssetIdByGUIName, const char*, (const char* guiName), (""),
   "Queries the Asset Database to see if any asset exists that is associated with the provided GUI Name.\n"
   "@return The AssetId of the associated asset, if any.")
{
   return GUIAsset::getAssetIdByGUIName(StringTable->insert(guiName));
}

DefineEngineMethod(GUIAsset, getScriptPath, const char*, (), ,
   "Gets the script file path associated to this asset.\n"
   "@return The full script file path.")
{
   return object->getScriptPath();
}
#endif

//-----------------------------------------------------------------------------
// GuiInspectorTypeAssetId
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiInspectorTypeGUIAssetPtr);

ConsoleDocClass(GuiInspectorTypeGUIAssetPtr,
   "@brief Inspector field type for GUI Asset Objects\n\n"
   "Editor use only.\n\n"
   "@internal"
);

void GuiInspectorTypeGUIAssetPtr::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeGUIAssetPtr)->setInspectorFieldType("GuiInspectorTypeGUIAssetPtr");
}

GuiControl* GuiInspectorTypeGUIAssetPtr::constructEditControl()
{
   // Create base filename edit controls
   GuiControl *retCtrl = Parent::constructEditControl();
   if (retCtrl == NULL)
      return retCtrl;

   // Change filespec
   char szBuffer[512];
   dSprintf(szBuffer, sizeof(szBuffer), "AssetBrowser.showDialog(\"GUIAsset\", \"AssetBrowser.changeAsset\", %d, %s);",
      mInspector->getIdString(), mCaption);
   mBrowseButton->setField("Command", szBuffer);

   // Create "Open in ShapeEditor" button
   mSMEdButton = new GuiBitmapButtonCtrl();

   dSprintf(szBuffer, sizeof(szBuffer), "echo(\"Game Object Editor not implemented yet!\");", retCtrl->getId());
   mSMEdButton->setField("Command", szBuffer);

   char bitmapName[512] = "ToolsModule:GameTSCtrl_image";
   mSMEdButton->setBitmap(StringTable->insert(bitmapName));

   mSMEdButton->setDataField(StringTable->insert("Profile"), NULL, "GuiButtonProfile");
   mSMEdButton->setDataField(StringTable->insert("tooltipprofile"), NULL, "GuiToolTipProfile");
   mSMEdButton->setDataField(StringTable->insert("hovertime"), NULL, "1000");
   mSMEdButton->setDataField(StringTable->insert("tooltip"), NULL, "Open this file in the GUI Editor");

   mSMEdButton->registerObject();
   addObject(mSMEdButton);

   return retCtrl;
}

bool GuiInspectorTypeGUIAssetPtr::updateRects()
{
   S32 dividerPos, dividerMargin;
   mInspector->getDivider(dividerPos, dividerMargin);
   Point2I fieldExtent = getExtent();
   Point2I fieldPos = getPosition();

   mCaptionRect.set(0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y);
   mEditCtrlRect.set(fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 34, fieldExtent.y);

   bool resized = mEdit->resize(mEditCtrlRect.point, mEditCtrlRect.extent);
   if (mBrowseButton != NULL)
   {
      mBrowseRect.set(fieldExtent.x - 32, 2, 14, fieldExtent.y - 4);
      resized |= mBrowseButton->resize(mBrowseRect.point, mBrowseRect.extent);
   }

   if (mSMEdButton != NULL)
   {
      RectI shapeEdRect(fieldExtent.x - 16, 2, 14, fieldExtent.y - 4);
      resized |= mSMEdButton->resize(shapeEdRect.point, shapeEdRect.extent);
   }

   return resized;
}

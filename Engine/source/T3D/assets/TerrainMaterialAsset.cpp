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

#ifndef TERRAINMATERIALASSET_H
#include "TerrainMaterialAsset.h"
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

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(TerrainMaterialAsset);

ConsoleType(TerrainMaterialAssetPtr, TypeTerrainMaterialAssetPtr, TerrainMaterialAsset, ASSET_ID_FIELD_PREFIX)

//-----------------------------------------------------------------------------

ConsoleGetType(TypeTerrainMaterialAssetPtr)
{
   // Fetch asset Id.
   return (*((AssetPtr<TerrainMaterialAsset>*)dptr)).getAssetId();
}

//-----------------------------------------------------------------------------

ConsoleSetType(TypeTerrainMaterialAssetPtr)
{
   // Was a single argument specified?
   if (argc == 1)
   {
      // Yes, so fetch field value.
      const char* pFieldValue = argv[0];

      // Fetch asset pointer.
      AssetPtr<TerrainMaterialAsset>* pAssetPtr = dynamic_cast<AssetPtr<TerrainMaterialAsset>*>((AssetPtrBase*)(dptr));

      // Is the asset pointer the correct type?
      if (pAssetPtr == NULL)
      {
         // No, so fail.
         //Con::warnf("(TypeMaterialAssetPtr) - Failed to set asset Id '%d'.", pFieldValue);
         return;
      }

      // Set asset.
      pAssetPtr->setAssetId(pFieldValue);

      return;
   }

   // Warn.
   Con::warnf("(TypeTerrainMaterialAssetPtr) - Cannot set multiple args to a single asset.");
}

//-----------------------------------------------------------------------------

TerrainMaterialAsset::TerrainMaterialAsset()
{
   mScriptFile = StringTable->EmptyString();
   mScriptPath = StringTable->EmptyString();
   mMatDefinitionName = StringTable->EmptyString();
}

//-----------------------------------------------------------------------------

TerrainMaterialAsset::~TerrainMaterialAsset()
{
}

//-----------------------------------------------------------------------------

void TerrainMaterialAsset::initPersistFields()
{
   // Call parent.
   Parent::initPersistFields();

   //addField("shaderGraph", TypeRealString, Offset(mShaderGraphFile, TerrainMaterialAsset), "");
   addProtectedField("scriptFile", TypeAssetLooseFilePath, Offset(mScriptFile, TerrainMaterialAsset),
      &setScriptFile, &getScriptFile, "Path to the file containing the material definition.");

   addField("materialDefinitionName", TypeString, Offset(mMatDefinitionName, TerrainMaterialAsset), "Name of the material definition this asset is for.");
}

void TerrainMaterialAsset::initializeAsset()
{
   // Call parent.
   Parent::initializeAsset();

   mScriptPath = getOwned() ? expandAssetFilePath(mScriptFile) : mScriptPath;

   if (Torque::FS::IsScriptFile(mScriptPath))
      Con::executeFile(mScriptPath, false, false);
}

void TerrainMaterialAsset::onAssetRefresh()
{
   mScriptPath = getOwned() ? expandAssetFilePath(mScriptFile) : mScriptPath;

   if (Torque::FS::IsScriptFile(mScriptPath))
      Con::executeFile(mScriptPath, false, false);

   if (mMatDefinitionName != StringTable->EmptyString())
   {
      TerrainMaterial* matDef;
      if (!Sim::findObject(mMatDefinitionName, matDef))
      {
         Con::errorf("TerrainMaterialAsset: Unable to find the Material %s", mMatDefinitionName);
         return;
      }

      //matDef->reload();
   }
}

void TerrainMaterialAsset::setScriptFile(const char* pScriptFile)
{
   // Sanity!
   AssertFatal(pScriptFile != NULL, "Cannot use a NULL script file.");

   pScriptFile = StringTable->insert(pScriptFile, true);

   // Ignore no change,
   if (pScriptFile == mScriptFile)
      return;

   // Update.
   mScriptFile = getOwned() ? expandAssetFilePath(pScriptFile) : pScriptFile;

   // Refresh the asset.
   refreshAsset();
}

//------------------------------------------------------------------------------

void TerrainMaterialAsset::copyTo(SimObject* object)
{
   // Call to parent.
   Parent::copyTo(object);
}

StringTableEntry TerrainMaterialAsset::getAssetIdByMaterialName(StringTableEntry matName)
{
   StringTableEntry materialAssetId = StringTable->EmptyString();

   AssetQuery* query = new AssetQuery();
   U32 foundCount = AssetDatabase.findAssetType(query, "TerrainMaterialAsset");
   if (foundCount == 0)
   {
      //Didn't work, so have us fall back to a placeholder asset
      materialAssetId = StringTable->insert("Core_Rendering:noMaterial");
   }
   else
   {
      for (U32 i = 0; i < foundCount; i++)
      {
         TerrainMaterialAsset* matAsset = AssetDatabase.acquireAsset<TerrainMaterialAsset>(query->mAssetList[i]);
         if (matAsset && matAsset->getMaterialDefinitionName() == matName)
         {
            materialAssetId = matAsset->getAssetId();
            AssetDatabase.releaseAsset(query->mAssetList[i]);
            break;
         }
         AssetDatabase.releaseAsset(query->mAssetList[i]);
      }
   }

   return materialAssetId;
}

#ifdef TORQUE_TOOLS
DefineEngineStaticMethod(TerrainMaterialAsset, getAssetIdByMaterialName, const char*, (const char* materialName), (""),
   "Queries the Asset Database to see if any asset exists that is associated with the provided material name.\n"
   "@return The AssetId of the associated asset, if any.")
{
   return TerrainMaterialAsset::getAssetIdByMaterialName(StringTable->insert(materialName));
}
#endif
//-----------------------------------------------------------------------------
// GuiInspectorTypeAssetId
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiInspectorTypeTerrainMaterialAssetPtr);

ConsoleDocClass(GuiInspectorTypeTerrainMaterialAssetPtr,
   "@brief Inspector field type for Material Asset Objects\n\n"
   "Editor use only.\n\n"
   "@internal"
);

void GuiInspectorTypeTerrainMaterialAssetPtr::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeTerrainMaterialAssetPtr)->setInspectorFieldType("GuiInspectorTypeTerrainMaterialAssetPtr");
}

GuiControl* GuiInspectorTypeTerrainMaterialAssetPtr::constructEditControl()
{
   // Create base filename edit controls
   mUseHeightOverride = true;
   mHeightOverride = 100;

   mMatEdContainer = new GuiControl();
   mMatEdContainer->registerObject();

   addObject(mMatEdContainer);

   // Create "Open in ShapeEditor" button
   mMatPreviewButton = new GuiBitmapButtonCtrl();

   const char* matAssetId = getData();

   TerrainMaterialAsset* matAsset = AssetDatabase.acquireAsset< TerrainMaterialAsset>(matAssetId);

   TerrainMaterial* materialDef = nullptr;

   char bitmapName[512] = "ToolsModule:material_editor_n_image";

   /*if (!Sim::findObject(matAsset->getMaterialDefinitionName(), materialDef))
   {
      Con::errorf("GuiInspectorTypeTerrainMaterialAssetPtr::constructEditControl() - unable to find material in asset");
   }
   else
   {
      mMatPreviewButton->setBitmap(materialDef->mDiffuseMapFilename[0]);
   }*/

   mMatPreviewButton->setPosition(0, 0);
   mMatPreviewButton->setExtent(100,100);

   // Change filespec
   char szBuffer[512];
   dSprintf(szBuffer, sizeof(szBuffer), "AssetBrowser.showDialog(\"TerrainMaterialAsset\", \"AssetBrowser.changeAsset\", %d, %s);",
      mInspector->getIdString(), mCaption);
   mMatPreviewButton->setField("Command", szBuffer);

   mMatPreviewButton->setDataField(StringTable->insert("Profile"), NULL, "GuiButtonProfile");
   mMatPreviewButton->setDataField(StringTable->insert("tooltipprofile"), NULL, "GuiToolTipProfile");
   mMatPreviewButton->setDataField(StringTable->insert("hovertime"), NULL, "1000");

   StringBuilder strbld;
   strbld.append(matAsset->getMaterialDefinitionName());
   strbld.append("\n");
   strbld.append("Open this asset in the Material Editor");

   mMatPreviewButton->setDataField(StringTable->insert("tooltip"), NULL, strbld.data());

   _registerEditControl(mMatPreviewButton);
   //mMatPreviewButton->registerObject();
   mMatEdContainer->addObject(mMatPreviewButton);

   mMatAssetIdTxt = new GuiTextEditCtrl();
   mMatAssetIdTxt->registerObject();
   mMatAssetIdTxt->setActive(false);

   mMatAssetIdTxt->setText(matAssetId);

   mMatAssetIdTxt->setBounds(100, 0, 150, 18);
   mMatEdContainer->addObject(mMatAssetIdTxt);

   return mMatEdContainer;
}

bool GuiInspectorTypeTerrainMaterialAssetPtr::updateRects()
{
   S32 dividerPos, dividerMargin;
   mInspector->getDivider(dividerPos, dividerMargin);
   Point2I fieldExtent = getExtent();
   Point2I fieldPos = getPosition();

   mCaptionRect.set(0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y);
   mEditCtrlRect.set(fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 34, fieldExtent.y);

   bool resized = mEdit->resize(mEditCtrlRect.point, mEditCtrlRect.extent);

   if (mMatEdContainer != nullptr)
   {
      mMatPreviewButton->resize(mEditCtrlRect.point, mEditCtrlRect.extent);
   }

   if (mMatPreviewButton != nullptr)
   {
      mMatPreviewButton->resize(Point2I::Zero, Point2I(100, 100));
   }

   if (mMatAssetIdTxt != nullptr)
   {
      mMatAssetIdTxt->resize(Point2I(100, 0), Point2I(mEditCtrlRect.extent.x - 100, 18));
   }

   return resized;
}

void GuiInspectorTypeTerrainMaterialAssetPtr::setMaterialAsset(String assetId)
{
   mTargetObject->setDataField(mCaption, "", assetId);

   //force a refresh
   SimObject* obj = mInspector->getInspectObject();
   mInspector->inspectObject(obj);
}

DefineEngineMethod(GuiInspectorTypeTerrainMaterialAssetPtr, setMaterialAsset, void, (String assetId), (""),
   "Gets a particular shape animation asset for this shape.\n"
   "@param animation asset index.\n"
   "@return Shape Animation Asset.\n")
{
   if (assetId == String::EmptyString)
      return;

   return object->setMaterialAsset(assetId);
}

#pragma once

#ifndef MENUBAR_COMMANDS_IDS_DEFINED
#define MENUBAR_COMMANDS_IDS_DEFINED

// TnzQt includes
#include "toonzqt/selectioncommandids.h"

/*!
  \file     menubarcommandids.h
  \brief    Contains string identifiers for Tnz6 commands.
*/

#define MI_NewScene "MI_NewScene"
#define MI_LoadScene "MI_LoadScene"
#define MI_SaveScene "MI_SaveScene"
#define MI_SaveSceneAs "MI_SaveSceneAs"
#define MI_SaveAll "MI_SaveAll"
#define MI_SaveAllLevels "MI_SaveAllLevels"
#define MI_RevertScene "MI_RevertScene"
#define MI_LoadSubSceneFile "MI_LoadSubSceneFile"

#define MI_OpenRecentScene "MI_OpenRecentScene"
#define MI_OpenRecentLevel "MI_OpenRecentLevel"
#define MI_ClearRecentScene "MI_ClearRecentScene"
#define MI_ClearRecentLevel "MI_ClearRecentLevel"
#define MI_PrintXsheet "MI_PrintXsheet"
#define MI_NewLevel "MI_NewLevel"
#define MI_NewVectorLevel "MI_NewVectorLevel"
#define MI_NewToonzRasterLevel "MI_NewToonzRasterLevel"
#define MI_NewRasterLevel "MI_NewRasterLevel"
#define MI_NewMetaLevel "MI_NewMetaLevel"
#define MI_LoadLevel "MI_LoadLevel"
#define MI_LoadFolder "MI_LoadFolder"
#define MI_SaveLevel "MI_SaveLevel"
#define MI_SaveLevelAs "MI_SaveLevelAs"
#define MI_ExportLevel "MI_ExportLevel"
#define MI_ExportAllLevels "MI_ExportAllLevels"
#define MI_SavePaletteAs "MI_SavePaletteAs"
#define MI_OverwritePalette "MI_OverwritePalette"
#define MI_LoadColorModel "MI_LoadColorModel"
#define MI_ImportMagpieFile "MI_ImportMagpieFile"
#define MI_NewNoteLevel "MI_NewNoteLevel"
#define MI_RemoveEmptyColumns "MI_RemoveEmptyColumns"
#define MI_NewProject "MI_NewProject"
#define MI_ProjectSettings "MI_ProjectSettings"
#define MI_SaveDefaultSettings "MI_SaveDefaultSettings"
#define MI_OutputSettings "MI_OutputSettings"
#define MI_PreviewSettings "MI_PreviewSettings"
#define MI_Render "MI_Render"
#define MI_FastRender "MI_FastRender"
#define MI_Preview "MI_Preview"
#define MI_SoundTrack "MI_SoundTrack"
#define MI_RegeneratePreview "MI_RegeneratePreview"
#define MI_RegenerateFramePr "MI_RegenerateFramePr"
#define MI_ClonePreview "MI_ClonePreview"
#define MI_FreezePreview "MI_FrezzePreview"
#define MI_SavePreviewedFrames "MI_SavePreviewedFrames"
// #define MI_SavePreview         "MI_SavePreview"
#define MI_ToggleViewerPreview "MI_ToggleViewerPreview"
#define MI_ToggleViewerSubCameraPreview "MI_ToggleViewerSubCameraPreview"
#define MI_Print "MI_Print"
#define MI_Preferences "MI_Preferences"
#define MI_SavePreset "MI_SavePreset"
#define MI_ShortcutPopup "MI_ShortcutPopup"
#define MI_Quit "MI_Quit"

#define MI_Undo "MI_Undo"
#define MI_Redo "MI_Redo"

#define MI_DefineScanner "MI_DefineScanner"
#define MI_ScanSettings "MI_ScanSettings"
#define MI_Scan "MI_Scan"
#define MI_Autocenter "MI_Autocenter"
#define MI_SetScanCropbox "MI_SetScanCropbox"
#define MI_ResetScanCropbox "MI_ResetScanCropbox"
#define MI_CleanupSettings "MI_CleanupSettings"
#define MI_CleanupPreview "MI_CleanupPreview"
#define MI_CameraTest "MI_CameraTest"
#define MI_OpacityCheck "MI_OpacityCheck"
#define MI_Cleanup "MI_Cleanup"

#define MI_AddFrames "MI_AddFrames"
#define MI_Renumber "MI_Renumber"
#define MI_FileInfo "MI_FileInfo"
#define MI_LevelSettings "MI_LevelSettings"
#define MI_CanvasSize "MI_CanvasSize"
#define MI_RemoveUnused "MI_RemoveUnused"

// #define MI_OpenCurrentScene  "MI_OpenCurrentScene"
#define MI_OpenFileBrowser "MI_OpenFileBrowser"
#define MI_OpenPreproductionBoard "MI_OpenPreproductionBoard"
#define MI_OpenFileViewer "MI_OpenFileViewer"
#define MI_OpenFilmStrip "MI_OpenFilmStrip"
#define MI_OpenPalette "MI_OpenPalette"
#define MI_OpenPltGizmo "MI_OpenPltGizmo"
#define MI_OpenFileBrowser2 "MI_OpenFileBrowser2"
#define MI_OpenStyleControl "MI_OpenStyleControl"
#define MI_OpenToolbar "MI_OpenToolbar"
#define MI_OpenCommandToolbar "MI_OpenCommandToolbar"
#define MI_OpenToolOptionBar "MI_OpenToolOptionBar"
#define MI_OpenLevelView "MI_OpenLevelView"
#define MI_OpenStopMotionPanel "MI_OpenStopMotionPanel"
// #define MI_OpenExport "MI_OpenExport"
#define MI_BrightnessAndContrast "MI_BrightnessAndContrast"
#define MI_Antialias "MI_Antialias"
#define MI_AdjustLevels "MI_AdjustLevels"
#define MI_AdjustThickness "MI_AdjustThickness"
#define MI_Binarize "MI_Binarize"
#define MI_LinesFade "MI_LinesFade"
#define MI_OpenXshView "MI_OpenXshView"
#define MI_OpenTimelineView "MI_OpenTimelineView"
#define MI_OpenMessage "MI_OpenMessage"
#define MI_OpenTest "MI_OpenTest"
#define MI_OpenTasks "MI_OpenTasks"
#define MI_OpenBatchServers "MI_OpenBatchServers"
#define MI_OpenTMessage "MI_OpenTMessage"
#define MI_OpenColorModel "MI_OpenColorModel"
#define MI_OpenStudioPalette "MI_OpenStudioPalette"
#define MI_OpenSchematic "MI_OpenSchematic"

#define MI_Export "MI_Export"
#define MI_TestAnimation "MI_TestAnimation"

#define MI_SceneSettings "MI_SceneSettings"
#define MI_CameraSettings "MI_CameraSettings"
#define MI_CameraStage "MI_CameraStage"
#define MI_SaveSubxsheetAs "MI_SaveSubxsheetAs"
#define MI_Resequence "MI_Resequence"
#define MI_CloneChild "MI_CloneChild"
#define MI_ApplyMatchLines "MI_ApplyMatchLines"
#define MI_MergeCmapped "MI_MergeCmapped"
#define MI_MergeColumns "MI_MergeColumns"
#define MI_DeleteMatchLines "MI_DeleteMatchLines"
#define MI_DeleteInk "MI_DeleteInk"
#define MI_InsertSceneFrame "MI_InsertSceneFrame"
#define MI_RemoveSceneFrame "MI_RemoveSceneFrame"

#define MI_InsertGlobalKeyframe "MI_InsertGlobalKeyframe"
#define MI_RemoveGlobalKeyframe "MI_RemoveGlobalKeyframe"
#define MI_DrawingSubForward "MI_DrawingSubForward"
#define MI_DrawingSubBackward "MI_DrawingSubBackward"
#define MI_DrawingSubGroupForward "MI_DrawingSubGroupForward"
#define MI_DrawingSubGroupBackward "MI_DrawingSubGroupBackward"

#define MI_InsertFx "MI_InsertFx"
#define MI_NewOutputFx "MI_NewOutputFx"

#define MI_PasteNew "MI_PasteNew"
#define MI_Autorenumber "MI_Autorenumber"
#define MI_CreateBlankDrawing "MI_CreateBlankDrawing"
#define MI_FillEmptyCell "MI_FillEmptyCell"

#define MI_MergeFrames "MI_MergeFrames"
#define MI_Reverse "MI_Reverse"
#define MI_Swing "MI_Swing"
#define MI_Random "MI_Random"
#define MI_Increment "MI_Increment"
#define MI_Dup "MI_Dup"
#define MI_ResetStep "MI_ResetStep"
#define MI_IncreaseStep "MI_IncreaseStep"
#define MI_DecreaseStep "MI_DecreaseStep"
#define MI_Step2 "MI_Step2"
#define MI_Step3 "MI_Step3"
#define MI_Step4 "MI_Step4"
#define MI_Each2 "MI_Each2"
#define MI_Each3 "MI_Each3"
#define MI_Each4 "MI_Each4"
#define MI_Rollup "MI_Rollup"
#define MI_Rolldown "MI_Rolldown"
#define MI_TimeStretch "MI_TimeStretch"
#define MI_Duplicate "MI_Duplicate"
#define MI_CloneLevel "MI_CloneLevel"
#define MI_SetKeyframes "MI_SetKeyframes"

#define MI_ViewCamera "MI_ViewCamera"
#define MI_ViewBBox "MI_ViewBBox"
#define MI_ViewTable "MI_ViewTable"
#define MI_FieldGuide "MI_FieldGuide"
#define MI_RasterizePli "MI_RasterizePli"
#define MI_SafeArea "MI_SafeArea"
#define MI_ViewColorcard "MI_ViewColorcard"
#define MI_ViewGuide "MI_ViewGuide"
#define MI_ViewRuler "MI_ViewRuler"
#define MI_TCheck "MI_TCheck"
#define MI_ICheck "MI_ICheck"
#define MI_Ink1Check "MI_Ink1Check"
#define MI_PCheck "MI_PCheck"
#define MI_IOnly "MI_IOnly"
#define MI_BCheck "MI_BCheck"
#define MI_GCheck "MI_GCheck"
#define MI_ACheck "MI_ACheck"
#define MI_ShiftTrace "MI_ShiftTrace"
#define MI_EditShift "MI_EditShift"
#define MI_NoShift "MI_NoShift"
#define MI_ResetShift "MI_ResetShift"
#define MI_ShowShiftOrigin "MI_ShowShiftOrigin"
#define MI_Histogram "MI_Histogram"
#define MI_ViewerHistogram "MI_ViewerHistogram"
#define MI_FxParamEditor "MI_FxParamEditor"

#define MI_Link "MI_Link"
#define MI_Play "MI_Play"
#define MI_ShortPlay "MI_ShortPlay"
#define MI_Loop "MI_Loop"
#define MI_Pause "MI_Pause"
#define MI_FirstFrame "MI_FirstFrame"
#define MI_LastFrame "MI_LastFrame"
#define MI_NextFrame "MI_NextFrame"
#define MI_PrevFrame "MI_PrevFrame"
#define MI_NextDrawing "MI_NextDrawing"
#define MI_PrevDrawing "MI_PrevDrawing"
#define MI_NextStep "MI_NextStep"
#define MI_PrevStep "MI_PrevStep"
#define MI_NextKeyframe "MI_NextKeyframe"
#define MI_PrevKeyframe "MI_PrevKeyframe"
#define MI_ToggleBlankFrames "MI_ToggleBlankFrames"

#define MI_RedChannel "MI_RedChannel"
#define MI_GreenChannel "MI_GreenChannel"
#define MI_BlueChannel "MI_BlueChannel"
#define MI_MatteChannel "MI_MatteChannel"

#define MI_AutoFillToggle "MI_AutoFillToggle"
#define MI_RedChannelGreyscale "MI_RedChannelGreyscale"
#define MI_GreenChannelGreyscale "MI_GreenChannelGreyscale"
#define MI_BlueChannelGreyscale "MI_BlueChannelGreyscale"

#define MI_DockingCheck "MI_DockingCheck"

#define MI_OpenFileViewer "MI_OpenFileViewer"
#define MI_OpenFileBrowser2 "MI_OpenFileBrowser2"
#define MI_OpenStyleControl "MI_OpenStyleControl"
#define MI_OpenFunctionEditor "MI_OpenFunctionEditor"
#define MI_OpenLevelView "MI_OpenLevelView"
#define MI_OpenXshView "MI_OpenXshView"
#define MI_OpenTimelineView "MI_OpenTimelineView"
#define MI_OpenCleanupSettings "MI_OpenCleanupSettings"
#define MI_ResetRoomLayout "MI_ResetRoomLayout"
#define MI_MaximizePanel "MI_MaximizePanel"
#define MI_FullScreenWindow "MI_FullScreenWindow"
#define MI_SeeThroughWindow "MI_SeeThroughWindow"
#define MI_OnionSkin "MI_OnionSkin"
#define MI_ZeroThick "MI_ZeroThick"
#define MI_CursorOutline "MI_CursorOutline"
#define MI_ViewerIndicator "MI_ViewerIndicator"

// #define MI_LoadResourceFile       "MI_LoadResourceFile"
#define MI_DuplicateFile "MI_DuplicateFile"
#define MI_ViewFile "MI_ViewFile"
#define MI_ConvertFiles "MI_ConvertFiles"
#define MI_ConvertFileWithInput "MI_ConvertFileWithInput"
#define MI_ShowFolderContents "MI_ShowFolderContents"
#define MI_PremultiplyFile "MI_PremultiplyFile"
#define MI_AddToBatchRenderList "MI_AddToBatchRenderList"
#define MI_AddToBatchCleanupList "MI_AddToBatchCleanupList"
#define MI_ExposeResource "MI_ExposeResource"
#define MI_EditLevel "MI_EditLevel"
#define MI_ReplaceLevel "MI_ReplaceLevel"
#define MI_RevertToCleanedUp "MI_RevertToCleanedUp"
#define MI_RevertToLastSaved "MI_RevertToLastSaved"
#define MI_ConvertToVectors "MI_ConvertToVectors"
#define MI_ConvertToToonzRaster "MI_ConvertToToonzRaster"
#define MI_ConvertVectorToVector "MI_ConvertVectorToVector"
#define MI_Tracking "MI_Tracking"
#define MI_RemoveLevel "MI_RemoveLevel"
#define MI_CollectAssets "MI_CollectAssets"
#define MI_ImportScenes "MI_ImportScenes"
#define MI_ExportScenes "MI_ExportScenes"
#define MI_ExportCurrentScene "MI_ExportCurrentScene"

#define MI_SelectRowKeyframes "MI_SelectRowKeyframes"
#define MI_SelectColumnKeyframes "MI_SelectColumnKeyframes"
#define MI_SelectAllKeyframes "MI_SelectAllKeyframes"
#define MI_SelectAllKeyframesNotBefore "MI_SelectAllKeyframesNotBefore"
#define MI_SelectAllKeyframesNotAfter "MI_SelectAllKeyframesNotAfter"
#define MI_SelectPreviousKeysInColumn "MI_SelectPreviousKeysInColumn"
#define MI_SelectFollowingKeysInColumn "MI_SelectFollowingKeysInColumn"
#define MI_SelectPreviousKeysInRow "MI_SelectPreviousKeysInRow"
#define MI_SelectFollowingKeysInRow "MI_SelectFollowingKeysInRow"
#define MI_InvertKeyframeSelection "MI_InvertKeyframeSelection"

#define MI_ShiftKeyframesDown "MI_ShiftKeyframesDown"
#define MI_ShiftKeyframesUp "MI_ShiftKeyframesUp"

#define MI_SetAcceleration "MI_SetAcceleration"
#define MI_SetDeceleration "MI_SetDeceleration"
#define MI_SetConstantSpeed "MI_SetConstantSpeed"
#define MI_ResetInterpolation "MI_ResetInterpolation"

#define MI_UseLinearInterpolation "MI_UseLinearInterpolation"
#define MI_UseSpeedInOutInterpolation "MI_UseSpeedInOutInterpolation"
#define MI_UseEaseInOutInterpolation "MI_UseEaseInOutInterpolation"
#define MI_UseEaseInOutPctInterpolation "MI_UseEaseInOutPctInterpolation"
#define MI_UseExponentialInterpolation "MI_UseExponentialInterpolation"
#define MI_UseExpressionInterpolation "MI_UseExpressionInterpolation"
#define MI_UseFileInterpolation "MI_UseFileInterpolation"
#define MI_UseConstantInterpolation "MI_UseConstantInterpolation"

#define MI_ActivateThisColumnOnly "MI_ActivateThisColumnOnly"
#define MI_ActivateSelectedColumns "MI_ActivateSelectedColumns"
#define MI_ActivateAllColumns "MI_ActivateAllColumns"
#define MI_DeactivateSelectedColumns "MI_DeactivateSelectedColumns"
#define MI_DeactivateAllColumns "MI_DeactivateAllColumns"
#define MI_ToggleColumnsActivation "MI_ToggleColumnsActivation"
#define MI_EnableThisColumnOnly "MI_EnableThisColumnOnly"
#define MI_EnableSelectedColumns "MI_EnableSelectedColumns"
#define MI_EnableAllColumns "MI_EnableAllColumns"
#define MI_DisableAllColumns "MI_DisableAllColumns"
#define MI_DisableSelectedColumns "MI_DisableSelectedColumns"
#define MI_SwapEnabledColumns "MI_SwapEnabledColumns"
#define MI_LockThisColumnOnly "MI_LockThisColumnOnly"
#define MI_LockSelectedColumns "MI_LockSelectedColumns"
#define MI_LockAllColumns "MI_LockAllColumns"
#define MI_UnlockSelectedColumns "MI_UnlockSelectedColumns"
#define MI_UnlockAllColumns "MI_UnlockAllColumns"
#define MI_ToggleColumnLocks "MI_ToggleColumnLocks"
#define MI_ToggleXSheetToolbar "MI_ToggleXSheetToolbar"
#define MI_ToggleXsheetBreadcrumbs "MI_ToggleXsheetBreadcrumbs"
#define MI_FoldColumns "MI_FoldColumns"
#define MI_ToggleXsheetCameraColumn "MI_ToggleXsheetCameraColumn"
#define MI_ToggleCurrentTimeIndicator "MI_ToggleCurrentTimeIndicator"

#define MI_LoadIntoCurrentPalette "MI_LoadIntoCurrentPalette"
#define MI_AdjustCurrentLevelToPalette "MI_AdjustCurrentLevelToPalette"
#define MI_MergeToCurrentPalette "MI_MergeToCurrentPalette"
#define MI_ReplaceWithCurrentPalette "MI_ReplaceWithCurrentPalette"
#define MI_DeletePalette "MI_DeletePalette"
#define MI_RefreshTree "MI_RefreshTree"
#define MI_LoadRecentImage "MI_LoadRecentImage"
#define MI_ClearRecentImage "MI_ClearRecentImage"

#define MI_OpenComboViewer "MI_OpenComboViewer"
#define MI_OpenHistoryPanel "MI_OpenHistoryPanel"
#define MI_ReplaceParentDirectory "MI_ReplaceParentDirectory"
#define MI_Reframe1 "MI_Reframe1"
#define MI_Reframe2 "MI_Reframe2"
#define MI_Reframe3 "MI_Reframe3"
#define MI_Reframe4 "MI_Reframe4"
#define MI_ReframeWithEmptyInbetweens "MI_ReframeWithEmptyInbetweens"

#define MI_EditNextMode "MI_EditNextMode"
#define MI_EditPosition "MI_EditPosition"
#define MI_EditRotation "MI_EditRotation"
#define MI_EditScale "MI_EditScale"
#define MI_EditShear "MI_EditShear"
#define MI_EditCenter "MI_EditCenter"
#define MI_EditAll "MI_EditAll"

#define MI_SelectionNextType "MI_SelectionNextType"
#define MI_SelectionRectangular "MI_SelectionRectangular"
#define MI_SelectionFreehand "MI_SelectionFreehand"
#define MI_SelectionPolyline "MI_SelectionPolyline"

#define MI_GeometricNextShape "MI_GeometricNextShape"
#define MI_GeometricRectangle "MI_GeometricRectangle"
#define MI_GeometricCircle "MI_GeometricCircle"
#define MI_GeometricEllipse "MI_GeometricEllipse"
#define MI_GeometricLine "MI_GeometricLine"
#define MI_GeometricPolyline "MI_GeometricPolyline"
#define MI_GeometricArc "MI_GeometricArc"
#define MI_GeometricMultiArc "MI_GeometricMultiArc"
#define MI_GeometricPolygon "MI_GeometricPolygon"

#define MI_TypeNextStyle "MI_TypeNextStyle"
#define MI_TypeOblique "MI_TypeOblique"
#define MI_TypeRegular "MI_TypeRegular"
#define MI_TypeBoldOblique "MI_TypeBoldOblique"
#define MI_TypeBold "MI_TypeBold"

#define MI_PaintBrushNextMode "MI_PaintBrushNextMode"
#define MI_PaintBrushAreas "MI_PaintBrushAreas"
#define MI_PaintBrushLines "MI_PaintBrushLines"
#define MI_PaintBrushLinesAndAreas "MI_PaintBrushLinesAndAreas"

#define MI_FillNextType "MI_FillNextType"
#define MI_FillNormal "MI_FillNormal"
#define MI_FillRectangular "MI_FillRectangular"
#define MI_FillFreehand "MI_FillFreehand"
#define MI_FillPolyline "MI_FillPolyline"
#define MI_FillFreepick "MI_FillFreepick"
#define MI_FillNextMode "MI_FillNextMode"
#define MI_FillAreas "MI_FillAreas"
#define MI_FillLines "MI_FillLines"
#define MI_FillLinesAndAreas "MI_FillLinesAndAreas"

#define MI_EraserNextType "MI_EraserNextType"
#define MI_EraserNormal "MI_EraserNormal"
#define MI_EraserRectangular "MI_EraserRectangular"
#define MI_EraserFreehand "MI_EraserFreehand"
#define MI_EraserPolyline "MI_EraserPolyline"
#define MI_EraserSegment "MI_EraserSegment"
#define MI_EraserMultiArc "MI_EraserMultiArc"

#define MI_TapeNextType "MI_TapeNextType"
#define MI_TapeNormal "MI_TapeNormal"
#define MI_TapeRectangular "MI_TapeRectangular"
#define MI_TapeNextMode "MI_TapeNextMode"
#define MI_TapeEndpointToEndpoint "MI_TapeEndpointToEndpoint"
#define MI_TapeEndpointToLine "MI_TapeEndpointToLine"
#define MI_TapeLineToLine "MI_TapeLineToLine"

#define MI_PickStyleNextMode "MI_PickStyleNextMode"
#define MI_PickStyleAreas "MI_PickStyleAreas"
#define MI_PickStyleLines "MI_PickStyleLines"
#define MI_PickStyleLinesAndAreas "MI_PickStyleLinesAndAreas"

#define MI_RGBPickerNextType "MI_RGBPickerNextType"
#define MI_RGBPickerNormal "MI_RGBPickerNormal"
#define MI_RGBPickerRectangular "MI_RGBPickerRectangular"
#define MI_RGBPickerFreehand "MI_RGBPickerFreehand"
#define MI_RGBPickerPolyline "MI_RGBPickerPolyline"

#define MI_SkeletonNextMode "MI_SkeletonNextMode"
#define MI_SkeletonBuildSkeleton "MI_SkeletonBuildSkeleton"
#define MI_SkeletonAnimate "MI_SkeletonAnimate"
#define MI_SkeletonInverseKinematics "MI_SkeletonInverseKinematics"

#define MI_PlasticNextMode "MI_PlasticNextMode"
#define MI_PlasticEditMesh "MI_PlasticEditMesh"
#define MI_PlasticPaintRigid "MI_PlasticPaintRigid"
#define MI_PlasticBuildSkeleton "MI_PlasticBuildSkeleton"
#define MI_PlasticAnimate "MI_PlasticAnimate"

#define MI_DeactivateUpperColumns "MI_DeactivateUpperColumns"
#define MI_CompareToSnapshot "MI_CompareToSnapshot"
#define MI_PreviewFx "MI_PreviewFx"

#define MI_About "MI_About"
#define MI_StartupPopup "MI_StartupPopup"
#define MI_PencilTest "MI_PencilTest"
#define MI_AudioRecording "MI_AudioRecording"
#define MI_LipSyncPopup "MI_LipSyncPopup"
#define MI_AutoLipSyncPopup "MI_AutoLipSyncPopup"
#define MI_AutoInputCellNumber "MI_AutoInputCellNumber"
#define MI_TouchGestureControl "MI_TouchGestureControl"
#define MI_SeparateColors "MI_SeparateColors"

#define MI_StopMotionCapture "MI_StopMotionCapture"
#define MI_StopMotionRaiseOpacity "MI_StopMotionRaiseOpacity"
#define MI_StopMotionLowerOpacity "MI_StopMotionLowerOpacity"
#define MI_StopMotionToggleLiveView "MI_StopMotionToggleLiveView"
#define MI_StopMotionToggleZoom "MI_StopMotionToggleZoom"
#define MI_StopMotionToggleUseLiveViewImages                                   \
  "MI_StopMotionToggleUseLiveViewImages"
#define MI_StopMotionLowerSubsampling "MI_StopMotionLowerSubsampling"
#define MI_StopMotionRaiseSubsampling "MI_StopMotionRaiseSubsampling"
#define MI_StopMotionJumpToCamera "MI_StopMotionJumpToCamera"
#define MI_StopMotionPickFocusCheck "MI_StopMotionPickFocusCheck"
#define MI_StopMotionExportImageSequence "MI_StopMotionExportImageSequence"
#define MI_StopMotionRemoveFrame "MI_StopMotionRemoveFrame"
#define MI_StopMotionNextFrame "MI_StopMotionNextFrame"

#define MI_OpenOnlineManual "MI_OpenOnlineManual"
#define MI_OpenWhatsNew "MI_OpenWhatsNew"
#define MI_OpenCommunityForum "MI_OpenCommunityForum"
#define MI_OpenReportABug "MI_OpenReportABug"

#define MI_ClearCacheFolder "MI_ClearCacheFolder"

#define MI_VectorGuidedDrawing "MI_VectorGuidedDrawing"
#define MI_OpenGuidedDrawingControls "MI_OpenGuidedDrawingControls"

#define MI_SelectNextGuideStroke "MI_SelectNextGuideStroke"
#define MI_SelectPrevGuideStroke "MI_SelectPrevGuideStroke"
#define MI_SelectGuideStrokeReset "MI_SelectGuideStrokeReset"
#define MI_TweenGuideStrokes "MI_TweenGuideStrokes"
#define MI_TweenGuideStrokeToSelected "MI_TweenGuideStrokeToSelected"
#define MI_SelectBothGuideStrokes "MI_SelectBothGuideStrokes"
#define MI_SelectGuidesAndTweenMode "MI_SelectGuidesAndTweenMode"

#define MI_FlipNextGuideStroke "MI_FlipNextGuideStroke"
#define MI_FlipPrevGuideStroke "MI_FlipPrevGuideStroke"

#define MI_ExportXDTS "MI_ExportXDTS"
#define MI_ExportOCA "MI_ExportOCA"
#define MI_ImportOCA "MI_ImportOCA"
#define MI_ExportTvpJson "MI_ExportTvpJson"
#define MI_ExportXsheetPDF "MI_ExportXsheetPDF"
#define MI_ExportCameraTrack "MI_ExportCameraTrack"

// mark id is added for each actual command (i.g. MI_SetCellMark1)
#define MI_SetCellMark "MI_SetCellMark"

#define MI_ZoomInAndFitPanel "MI_ZoomInAndFitPanel"
#define MI_ZoomOutAndFitPanel "MI_ZoomOutAndFitPanel"

#define MI_OpenCustomPanels "MI_OpenCustomPanels"
#define MI_CustomPanelEditor "MI_CustomPanelEditor"

#define MI_ConvertTZPInFolder "MI_ConvertTZPInFolder"

// Navigation tags
#define MI_ToggleTaggedFrame "MI_ToggleTaggedFrame"
#define MI_EditTaggedFrame "MI_EditTaggedFrame"
#define MI_NextTaggedFrame "MI_NextTaggedFrame"
#define MI_PrevTaggedFrame "MI_PrevTaggedFrame"
#define MI_ClearTags "MI_ClearTags"

#define MI_OpenLocator "MI_OpenLocator"
#endif

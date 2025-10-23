#include "toonz/tlog.h"  // For TSysLog::error
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"

//=============================================================================
// TSceneHandle
//-----------------------------------------------------------------------------

TSceneHandle::TSceneHandle() : m_scene(nullptr), m_dirtyFlag(false) {}

//-----------------------------------------------------------------------------

TSceneHandle::~TSceneHandle() {
  if (m_scene) {
    delete m_scene;
    m_scene = nullptr;
  }
}

//-----------------------------------------------------------------------------

ToonzScene* TSceneHandle::getScene() const { return m_scene; }

//-----------------------------------------------------------------------------

void TSceneHandle::setScene(ToonzScene* scene) {
  if (!scene) {
    TSysLog::error("TSceneHandle::setScene: Invalid ToonzScene pointer");
    return;
  }

  if (m_scene == scene) return;

  ToonzScene* oldscene = m_scene;
  m_scene              = scene;

  // Notify that the scene has been switched
  notifySceneSwitched();

  // Safely delete the old scene
  if (oldscene) {
    delete oldscene;
  }
}

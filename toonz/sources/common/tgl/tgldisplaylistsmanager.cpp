

// TnzCore includes
#include "tsmartpointer.h"

// tcg includes
#include "tcg/tcg_list.h"

#include "tgldisplaylistsmanager.h"
#include <mutex>

//***********************************************************************************************
//    Local namespace - declarations
//***********************************************************************************************

namespace {

struct ProxyReference {
  TGLDisplayListsProxy *m_proxy;
  int m_refCount;

  ProxyReference(TGLDisplayListsProxy *proxy) : m_proxy(proxy), m_refCount(0) {}
};

}  // namespace

//***********************************************************************************************
//    Local namespace - globals
//***********************************************************************************************

namespace {

tcg::list<ProxyReference> m_proxies;
std::map<TGlContext, int> m_proxyIdsByContext;
std::mutex m_mutex; // Thread safety mutex for concurrent access protection

}  // namespace

//***********************************************************************************************
//    TGLDisplayListsManager  implementation
//***********************************************************************************************

TGLDisplayListsManager *TGLDisplayListsManager::instance() {
  static TGLDisplayListsManager theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------------------

int TGLDisplayListsManager::storeProxy(TGLDisplayListsProxy *proxy) {
  std::lock_guard<std::mutex> lock(m_mutex); // Thread-safe operation
  return m_proxies.push_back(ProxyReference(proxy));
}

//-----------------------------------------------------------------------------------

void TGLDisplayListsManager::attachContext(int dlSpaceId, TGlContext context) {
  std::lock_guard<std::mutex> lock(m_mutex); // Thread-safe operation
  
  // Validate index bounds to prevent out-of-range access
  if (dlSpaceId < 0 || dlSpaceId >= static_cast<int>(m_proxies.size())) {
    return;
  }
  
  m_proxyIdsByContext.insert(std::make_pair(context, dlSpaceId));
  ++m_proxies[dlSpaceId].m_refCount;
}

//-----------------------------------------------------------------------------------

void TGLDisplayListsManager::releaseContext(TGlContext context) {
  std::lock_guard<std::mutex> lock(m_mutex); // Thread-safe operation

  std::map<TGlContext, int>::iterator it = m_proxyIdsByContext.find(context);

  assert(it != m_proxyIdsByContext.end());
  if (it == m_proxyIdsByContext.end()) return;

  int dlSpaceId = it->second;
  
  // Additional safety check for valid index
  if (dlSpaceId < 0 || dlSpaceId >= static_cast<int>(m_proxies.size())) {
    m_proxyIdsByContext.erase(it);
    return;
  }

  if (--m_proxies[dlSpaceId].m_refCount <= 0) {
    // Notify observers first before destruction
    observers_container::const_iterator ot, oEnd = observers().end();
    for (ot = observers().begin(); ot != oEnd; ++ot)
      static_cast<Observer *>(*ot)->onDisplayListDestroyed(dlSpaceId);

    // FIX: Delete only if pointer is not null (was inverted condition)
    if (m_proxies[dlSpaceId].m_proxy) {
      delete m_proxies[dlSpaceId].m_proxy;
      m_proxies[dlSpaceId].m_proxy = nullptr; // Prevent potential double deletion
    }
    m_proxies.erase(dlSpaceId);
  }

  m_proxyIdsByContext.erase(it);
}

//-----------------------------------------------------------------------------------

int TGLDisplayListsManager::displayListsSpaceId(TGlContext context) {
  std::lock_guard<std::mutex> lock(m_mutex); // Thread-safe operation
  std::map<TGlContext, int>::iterator it = m_proxyIdsByContext.find(context);
  return (it == m_proxyIdsByContext.end()) ? -1 : it->second;
}

//-----------------------------------------------------------------------------------

TGLDisplayListsProxy *TGLDisplayListsManager::dlProxy(int dlSpaceId) {
  std::lock_guard<std::mutex> lock(m_mutex); // Thread-safe operation
  
  // Add bounds checking to prevent out-of-range access
  if (dlSpaceId < 0 || dlSpaceId >= static_cast<int>(m_proxies.size())) {
    return nullptr;
  }
  return m_proxies[dlSpaceId].m_proxy;
}
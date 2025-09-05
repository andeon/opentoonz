

#include "tparamcontainer.h"
//#include "tdoubleparam.h"
#include "tparamset.h"

#include "tparam.h"

class TParamVar::Imp {
public:
  std::string m_name;
  bool m_isHidden;
  bool m_isObsolete;
  TParamObserver* m_paramObserver;

  Imp(std::string name, bool hidden, bool obsolete)
      : m_name(name), m_isHidden(hidden), m_isObsolete(obsolete), m_paramObserver(0) {}
};

class TParamContainer::Imp {
public:
  std::map<std::string, TParamVar*> m_nameTable;
  std::vector<std::unique_ptr<TParamVar>> m_vars;
  TParamObserver* m_paramObserver;

  Imp() : m_paramObserver(0) {}
};

// TParamVar implementation
TParamVar::TParamVar(std::string name, bool hidden, bool obsolete)
    : m_imp(std::make_unique<Imp>(name, hidden, obsolete)) {}

TParamVar::~TParamVar() = default;

std::string TParamVar::getName() const { return m_imp->m_name; }
bool TParamVar::isHidden() const { return m_imp->m_isHidden; }
void TParamVar::setIsHidden(bool hidden) { m_imp->m_isHidden = hidden; }
bool TParamVar::isObsolete() const { return m_imp->m_isObsolete; }

void TParamVar::setParamObserver(TParamObserver* obs) {
  if (m_imp->m_paramObserver == obs) return;
  TParam* param = getParam();
  if (param) {
    if (obs) param->addObserver(obs);
    if (m_imp->m_paramObserver) param->removeObserver(m_imp->m_paramObserver);
  }
  m_imp->m_paramObserver = obs;
}

// TParamContainer implementation
TParamContainer::TParamContainer() : m_imp(std::make_unique<Imp>()) {}
TParamContainer::~TParamContainer() = default;

void TParamContainer::add(std::unique_ptr<TParamVar> var) {
  m_imp->m_nameTable[var->getName()] = var.get(); // Store raw pointer
  m_imp->m_vars.push_back(std::move(var)); // Move unique_ptr into vector
  m_imp->m_vars.back()->setParamObserver(m_imp->m_paramObserver);
  m_imp->m_vars.back()->getParam()->setName(m_imp->m_vars.back()->getName());
}

int TParamContainer::getParamCount() const {
  return static_cast<int>(m_imp->m_vars.size());
}

TParam* TParamContainer::getParam(int index) const {
  assert(0 <= index && index < getParamCount());
  return m_imp->m_vars[index]->getParam();
}

bool TParamContainer::isParamHidden(int index) const {
  assert(0 <= index && index < getParamCount());
  return m_imp->m_vars[index]->isHidden();
}

std::string TParamContainer::getParamName(int index) const {
  assert(0 <= index && index < getParamCount());
  return m_imp->m_vars[index]->getName();
}

const TParamVar* TParamContainer::getParamVar(int index) const {
  assert(0 <= index && index < getParamCount());
  return m_imp->m_vars[index].get();
}

TParam* TParamContainer::getParam(std::string name) const {
  TParamVar* var = getParamVar(name);
  return (var) ? var->getParam() : 0;
}

TParamVar* TParamContainer::getParamVar(std::string name) const {
  auto it = m_imp->m_nameTable.find(name);
  if (it == m_imp->m_nameTable.end())
    return 0;
  else
    return it->second;
}

void TParamContainer::unlink() {
  for (int i = 0; i < getParamCount(); i++) {
    TParamVar* var = m_imp->m_vars[i].get();
    TParam* param = var->getParam();
    // p0 = dynamic_cast<TRangeParam *>(param);
    var->setParam(param->clone());
    /*p1 = dynamic_cast<TRangeParam *>(var->getParam());
    0 && p1)
    
    ng name = p0->getName();
     = p1->getName();
    }*/
  }
}

void TParamContainer::link(const TParamContainer* src) {
  assert(getParamCount() == src->getParamCount());
  for (int i = 0; i < getParamCount(); i++) {
    assert(getParamName(i) == src->getParamName(i));
    assert(m_imp->m_vars[i]->getName() == getParamName(i));
    m_imp->m_vars[i]->setParam(src->getParam(i));
  }
}

void TParamContainer::copy(const TParamContainer* src) {
  assert(getParamCount() == src->getParamCount());
  for (int i = 0; i < getParamCount(); i++) {
    assert(getParamName(i) == src->getParamName(i));
    assert(m_imp->m_vars[i]->getName() == getParamName(i));
    getParam(i)->copy(src->getParam(i));
  }
}


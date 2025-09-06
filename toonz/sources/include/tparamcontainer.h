#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#ifndef TPARAMCONTAINER_INCLUDED
#define TPARAMCONTAINER_INCLUDED

#include <memory>
#include <string>

#include "tparam.h"
// #include "tfx.h"
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TIStream;
class TOStream;
class TParamObserver;
class TParam;

class DVAPI TParamVar {
private:
  class Imp; // Forward declaration inside TParamVar
  std::unique_ptr<Imp> m_imp; // PIMPL to hide implementation details

public:
  TParamVar(std::string name, bool hidden = false, bool obsolete = false);
  virtual ~TParamVar();

  virtual TParamVar* clone() const = 0;
  std::string getName() const;
  bool isHidden() const;
  void setIsHidden(bool hidden);
  bool isObsolete() const;
  virtual void setParam(TParam* param) = 0;
  virtual TParam* getParam() const = 0;
  void setParamObserver(TParamObserver* obs);
};

template <class T>
class TParamVarT final : public TParamVar {
  // Very dirty fix for link fx, separating the variable between the plugin fx
  // and the built-in fx.
  // Note that for now link fx is available only with built-in fx, since m_var
  // must be "pointer to pointer" of parameter to make the link fx to work
  // properly.
  T* m_var;
  TParamP m_pluginVar;

public:
  TParamVarT(std::string name, T* var = 0, TParamP pluginVar = 0,
             bool hidden = false, bool obsolete = false)
      : TParamVar(name, hidden, obsolete), m_var(var), m_pluginVar(pluginVar) {}
  TParamVarT() = delete;

  void setParam(TParam* param) override {
    if (m_var)
      *m_var = TParamP(param);
    else
      m_pluginVar = TParamP(param);
  }

  TParam* getParam() const override {
    if (m_var)
      return m_var->getPointer();
    else
      return m_pluginVar.getPointer();
  }

  TParamVar* clone() const override {
    return new TParamVarT<T>(getName(), m_var, m_pluginVar, isHidden(),
                             isObsolete());
  }
};

class DVAPI TParamContainer {
private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TParamContainer();
  ~TParamContainer();

  void add(std::unique_ptr<TParamVar> var);

  int getParamCount() const;
  bool isParamHidden(int index) const;
  TParam* getParam(int index) const;
  std::string getParamName(int index) const;
  TParam* getParam(std::string name) const;
  TParamVar* getParamVar(std::string name) const;
  const TParamVar* getParamVar(int index) const;

  void unlink();
  void link(const TParamContainer* src);
  void copy(const TParamContainer* src);

  void setParamObserver(TParamObserver*);
  TParamObserver* getParamObserver() const;

private:
  TParamContainer(const TParamContainer&);
  TParamContainer& operator=(const TParamContainer&);
};

#endif  // TPARAMCONTAINER_INCLUDED

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#pragma once

#ifndef TFXPARAM_INCLUDED
#define TFXPARAM_INCLUDED

// #include "tcommon.h"

#include "tfx.h"
#include "tparamcontainer.h"

template <class T>
void bindParam(TFx *fx, std::string name, T &var, bool hidden = false,
               bool obsolete = false) {
  std::unique_ptr<TParamVar> paramVar = std::make_unique<TParamVarT<T>>(name, &var, TParamP(), hidden, obsolete);
  fx->getParams()->add(std::move(paramVar));
  var->addObserver(fx);
}

template <class T>
void bindPluginParam(TFx *fx, std::string name, T &var, bool hidden = false,
                     bool obsolete = false) {
  std::unique_ptr<TParamVar> paramVar = std::make_unique<TParamVarT<T>>(name, nullptr, var, hidden, obsolete);
  fx->getParams()->add(std::move(paramVar));
  var->addObserver(fx);
}

#endif

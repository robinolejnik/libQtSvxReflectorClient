#ifndef LIBQTSVXREFLECTORCLIENT_GLOBAL_H
#define LIBQTSVXREFLECTORCLIENT_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBQTSVXREFLECTORCLIENT_LIBRARY)
#  define LIBQTSVXREFLECTORCLIENTSHARED_EXPORT Q_DECL_EXPORT
#else
#  define LIBQTSVXREFLECTORCLIENTSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBQTSVXREFLECTORCLIENT_GLOBAL_H

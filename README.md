# QStyleLoader
Example of using library https://github.com/Engilian/QStyleLoaderExample 

Functions:
=========
- Automatic update of styles when changing files;
- Automatic reloading of component styles when updating a property. 

Example of use
==============

Headers
-------
```c++
#include <qstyle_loader.h>
// Or
#include "qstyle_loader.h"
```

Use auto reload style file
-------
```c++
QString path = QDir::fromNativeSeparators(
        QCoreApplication::applicationDirPath() +
        "/style.css" );

// Create QStyleLoader object
QStyleLoader style;

// Adding style file
style.addFile ( path );
```

Use auto reload style directory
-------
```c++
QString path = QDir::fromNativeSeparators(
        QCoreApplication::applicationDirPath() +
        "/style" );

// Create QStyleLoader object
QStyleLoader style;

// Adding style directory
style.addDirectory ( path );
```

Refresh the style of the child widget when its property changes. 
----------------------------------------------------------------
```c++
// Any widget
QMainWindow w;

QString path = QDir::fromNativeSeparators(
        QCoreApplication::applicationDirPath() +
        "/style.css" );

// Create QStyleLoader object
QStyleLoader style;

// Adding style file
style.addFile ( path );

// Create style update 
auto u = style.addUpdater( &w );

// Adding a tracked property
u->add ( "current" );

// Or set tratcked all properties
u->setUpdateWithAllChanges( true );

// Enable tracking of property child widgets (only when it is necessary to update child widgets). 
u->setRefreshChildWidgets( true );
```
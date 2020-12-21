#include "qstyle_loader.h"

#include <mutex>

#include <QMap>
#include <QSet>
#include <QDir>
#include <QFile>
#include <QStyle>
#include <QEvent>
#include <QDebug>
#include <QWidget>
#include <QLayout>
#include <QFileInfo>
#include <QDateTime>
#include <QChildEvent>
#include <QLayoutItem>
#include <QApplication>
#include <QDynamicPropertyChangeEvent>

/*
 *
 * QStyleUpdater
 *
 */

class QStyleUpdater::_QStyleUpdater
    : public QObject
{
  QStyleUpdater *m_root;
  QWidget       *m_widget;
  bool          m_updateChilds;
  bool          m_allProperties;
  QSet<QString> m_properties;
  std::function<bool(QWidget *)> m_filter;
  mutable std::recursive_mutex m_locker;
public:
  _QStyleUpdater(QStyleUpdater *root)
    : QObject( root )
    , m_root( root )
    , m_widget( nullptr )
    , m_updateChilds( false )
    , m_allProperties( false )
    , m_properties()
  {

  }
  ~_QStyleUpdater() override
  {

  }

public:
  QWidget *widget() const
  {
    return m_widget;
  }
  QStringList properties() const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    return m_properties.toList();
  }
  bool refreshChildWidgets() const
  {
    return m_updateChilds;
  }
  bool updateWithAllChanges() const
  {
    return m_allProperties;
  }

public slots:
  void reloadStyle()
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto w: getAllWidgets() )
      reloadWidgetStyle( w );
  }
  void setWidget(QWidget *widget)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto w: getAllWidgets() )
      w->removeEventFilter( this );

    m_widget = widget;
    for ( auto w: getAllWidgets() )
      w->installEventFilter( this );
  }
  void add(const QString &property)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_properties.insert( property );
  }
  void remove(const QString &property)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_properties.remove( property );
  }
  void setProperties(const QStringList &list)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_properties.clear();
    for ( auto &p: list )
      m_properties.insert( p );
  }
  void setRefreshChildWidgets(bool enable)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_updateChilds = enable;
  }
  void setUpdateWithAllChanges(bool enable)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_allProperties = enable;
  }
  void setChildFilter(const std::function<bool(QWidget *)> &filter)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_filter = filter;
  }

  // Widgets methods
private:
  bool checkChildWidget(QWidget *child)
  {
    try {
      return m_filter( child );
    } catch (...) { }
    return true;;
  };

  QList<QWidget*> getAllWidgets(QWidget *parent) const
  {
    QList<QWidget *> result;

    if ( !!parent ) {
      result << parent;

        for ( auto child: parent->findChildren<QWidget *>() ) {
          result.append( getAllWidgets( child ) );
        }

        if ( parent->layout() )
          result.append( getAllWidgets( parent->layout() ) );
    }

    return result;
  }

  QList<QWidget*> getAllWidgets(QLayout *layout) const
  {
    QList<QWidget*> result;

    for ( int i = 0; i < layout->count(); ++i )
      result.append( getAllWidgets( layout->itemAt( i ) ) );

    return result;
  }

  QList<QWidget*> getAllWidgets(QLayoutItem *item) const
  {
    QList<QWidget*> result;

    if ( item ? item->widget() : false )
      result.append( getAllWidgets( item->widget() ) );

    if ( item ? item->layout() : false )
      result.append( getAllWidgets( item->layout() ) );

    return result;
  }

  QList<QWidget*> getAllWidgets() const
  {
    return getAllWidgets( m_widget ).toSet().toList();
  }

  void reloadWidgetStyle(QWidget *widget)
  {
    widget->style()->unpolish( widget );
    widget->style()->polish( widget );
    emit m_root->styleReloaded( widget );
  }

  // QObject interface
protected:
  bool eventFilter(QObject *watcher, QEvent *event) override
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );

    auto watcherWidget = qobject_cast<QWidget*>( watcher );
    if ( !!watcherWidget ) {
      if ( event->type() == QEvent::Type::ChildAdded ) {
        auto e = dynamic_cast<QChildEvent*>( event );
        auto childWidget = qobject_cast<QWidget*>( e->child() );
        if ( childWidget ) {
          childWidget->installEventFilter( this );
        }
      } else if ( event->type() == QEvent::Type::ChildRemoved ) {
        auto e = dynamic_cast<QChildEvent*>( event );
        auto childWidget = qobject_cast<QWidget*>( e->child() );
        if ( childWidget ) {
          childWidget->removeEventFilter( this );
        }
      } else if ( event->type() == QEvent::Type::DynamicPropertyChange ) {
        auto e = dynamic_cast<QDynamicPropertyChangeEvent*>( event );
        if ( e->propertyName().indexOf( "_q_stylesheet" ) != 0  ) {
          if ( watcher == m_widget && ( m_allProperties || m_properties.contains( e->propertyName() ) ) ) {
            reloadWidgetStyle( m_widget );
          } else if ( m_updateChilds && ( m_allProperties || m_properties.contains( e->propertyName() ) ) ) {
            auto widget = qobject_cast<QWidget*>( watcher );
            if ( checkChildWidget( widget ) ) {
              reloadWidgetStyle( widget );
            }
          }
        }
      }
    }

    return QObject::eventFilter( watcher, event );
  }
};

QStyleUpdater::QStyleUpdater(QWidget *widget, QObject *parent)
  : QObject( parent )
  , ptr( new _QStyleUpdater( this ) )
{
  setWidget( widget );
}

QStyleUpdater::QStyleUpdater(const QStringList &properties, QWidget *widget, QObject *parent)
  : QObject( parent )
  , ptr( new _QStyleUpdater( this ) )
{
  setWidget( widget );
  setProperties( properties );
}

QStyleUpdater::QStyleUpdater(bool updateChilds, bool allProperies, QWidget *widget, QObject *parent)
  : QObject( parent )
  , ptr( new _QStyleUpdater( this ) )
{
  setWidget( widget );
  setRefreshChildWidgets( updateChilds );
  setUpdateWithAllChanges( allProperies );
}

QStyleUpdater::~QStyleUpdater()
{
  delete ptr;
}

QWidget *QStyleUpdater::widget() const
{
  return ptr->widget();
}

QStringList QStyleUpdater::properties() const
{
  return ptr->properties();
}

bool QStyleUpdater::refreshChildWidgets() const
{
  return ptr->refreshChildWidgets();
}

bool QStyleUpdater::updateWithAllChanges() const
{
  return ptr->updateWithAllChanges();
}

void QStyleUpdater::reloadStyle()
{
  ptr->reloadStyle();
}

void QStyleUpdater::setWidget(QWidget *widget)
{
  ptr->setWidget( widget );
}

void QStyleUpdater::add(const QString &property)
{
  ptr->add( property );
}

void QStyleUpdater::remove(const QString &property)
{
  ptr->remove( property );
}

void QStyleUpdater::setProperties(const QStringList &list)
{
  ptr->setProperties( list );
}

void QStyleUpdater::setRefreshChildWidgets(bool enable)
{
  ptr->setRefreshChildWidgets( enable );
}

void QStyleUpdater::setUpdateWithAllChanges(bool enable)
{
  ptr->setUpdateWithAllChanges( enable );
}

void QStyleUpdater::setChildFilter(const std::function<bool (QWidget *)> &filter)
{
  ptr->setChildFilter( filter );
}

/*
 *
 * QStyleLoader
 *
 */

class QStyleLoaderGuardObserver
{
protected:
  QStyleLoaderGuardObserver() { }
public:
  virtual ~QStyleLoaderGuardObserver() {}

public:
  virtual void added(const QString &) = 0;
  virtual void removed(const QString &) = 0;
  virtual void changed(const QString &) = 0;
};

class QStyleLoaderGuard
    : public QObject
{
protected:
  QString                   m_path;
  QStyleLoaderGuardObserver *m_observer;
protected:
  QStyleLoaderGuard(const QString &path, QStyleLoaderGuardObserver *observer, QObject *parent)
    : QObject( parent )
    , m_path( path )
    , m_observer( observer )
  {

  }
public:
  virtual ~QStyleLoaderGuard() override
  {

  }

public:
  QString path() const
  {
    return m_path;
  }
};

class QStyleLoaderFileGuard final
    : public QStyleLoaderGuard
{
  QDateTime m_lastEdit;
public:
  QStyleLoaderFileGuard(const QString &path, QStyleLoaderGuardObserver *observer, QObject *parent)
    : QStyleLoaderGuard( path, observer, parent )
    , m_lastEdit( QFileInfo( path ).lastModified() )
  {
    startTimer( 2500 );
  }
  ~QStyleLoaderFileGuard() override
  {

  }

public:
  bool exists() const
  {
    return !m_lastEdit.isNull();
  }

  // QObject interface
protected:
  void timerEvent(QTimerEvent *event) override
  {
    QFileInfo f ( m_path );
    if ( f.exists() ) {
      if ( m_lastEdit.isNull() ) {
        m_lastEdit = f.lastModified();
        m_observer->added( m_path );
      } else if ( m_lastEdit != f.lastModified() ) {
        m_lastEdit = f.lastModified();
        m_observer->changed( m_path );
      }
    } else if ( !m_lastEdit.isNull() ) {
      m_lastEdit = QDateTime();
      m_observer->removed( m_path );
    }

    QStyleLoaderGuard::timerEvent( event );
  }
};

class QStyleLoaderDirectoryGuard final
    : public QStyleLoaderGuard
{
  QStringList                                 m_filter;
  QMap<QString, QStyleLoaderFileGuard*>       m_files;
  QMap<QString, QStyleLoaderDirectoryGuard*>  m_dirs;
public:
  QStyleLoaderDirectoryGuard(const QString &path, const QStringList &filter, QStyleLoaderGuardObserver *observer, QObject *parent)
    : QStyleLoaderGuard( path, observer, parent )
    , m_filter( filter )
  {
    startTimer( 10000 );
  }
  ~QStyleLoaderDirectoryGuard() override
  {
    while ( m_dirs.isEmpty() )
      delete m_dirs.take( m_dirs.firstKey() );

    while ( m_files.isEmpty() )
      delete m_files.take( m_files.firstKey() );
  }

public:
  QStringList filter() const
  {
    return m_filter;
  }

  void setFilter(const QStringList &filter)
  {
    m_filter = filter;
    for ( auto &d: m_dirs.values() )
      d->setFilter( filter );

    updateEntries();
  }

private slots:
  void updateEntries()
  {
    QSet<QString> files, dirs;
    QDir directory( QDir::fromNativeSeparators( m_path ) );
    for ( auto file: directory.entryInfoList( m_filter, QDir::Files ) )
      files.insert( file.absoluteFilePath() );

    for ( auto name: directory.entryList( QDir::Dirs ) ) {
      if ( name == ".." ) continue;
      dirs.insert( directory.absoluteFilePath( name ) );
    }

    for ( auto &file: files ) {
      if ( !m_files.contains( file ) ) {
        m_files[ file ] = new QStyleLoaderFileGuard( file, m_observer, this );
        m_observer->added( file );
      }
    }

    for ( auto &file: m_files.keys() ) {
      if ( !files.contains( file ) ) {
        auto obj = m_files[ file ];
        m_files.remove( file );
        delete obj;

        m_observer->removed( file );
      }
    }

    for ( auto &dir: dirs )
      if ( !m_dirs.contains( dir ) )
        m_dirs[ dir ] = new QStyleLoaderDirectoryGuard( dir, m_filter, m_observer, this );

    for ( auto &dir: m_dirs.keys() ) {
      if ( !dirs.contains( dir ) ) {
        auto obj = m_dirs[ dir ];
        m_dirs.remove( dir );
        obj->deactivate();
        delete obj;
      }
    }
  }

  void deactivate()
  {
    for ( auto dir: m_dirs.values() )
      dir->deactivate();

    for ( auto file: m_files.values() )
      if ( file->exists() )
        m_observer->removed( file->path() );
  }

  // QObject interface
protected:
  void timerEvent(QTimerEvent *event) override
  {
    updateEntries();
    QStyleLoaderGuard::timerEvent( event );
  }
};

class QStyleLoader::_QStyleLoader
    : public QObject
    , public QStyleLoaderGuardObserver
{
  QStyleLoader                      *m_root;
  bool                              m_autoReload;
  bool                              m_hasReload;
  QDateTime                         m_lastReloaded;
  QList<Item>                       m_items;
  QStringList                       m_filter;
  QList<QStyleUpdater*>             m_updaters;
  QMap<QString, QStyleLoaderGuard*> m_guards;
  mutable std::recursive_mutex  m_locker;
public:
  _QStyleLoader(QStyleLoader *root)
    : QObject( root )
    , m_root( root )
    , m_autoReload( true )
    , m_hasReload( false )
  {
    startTimer( 2000 );
  }
  ~_QStyleLoader() override
  {

  }

public:
  int count() const
  {
    return m_items.count();
  }
  Item at(int index) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    return index >= 0 && index < m_items.count()
        ? m_items.at( index )
        : Item();
  }
  QList<Item> items() const
  {
    return m_items;
  }
  QStringList fileFilters() const
  {
    return m_filter;
  }

  bool contains(const QString &path) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto &item: m_items )
      if ( item.path == path )
        return true;
    return false;
  }
  bool containsFile(const QString &path) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto &item: m_items )
      if ( item.type == Item::Type::File && item.path == path )
        return true;
    return false;
  }
  bool containsDirectory(const QString &path) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto &item: m_items )
      if ( item.type == Item::Type::Directory && item.path == path )
        return true;
    return false;
  }

  QList<QStyleUpdater*> updaters() const
  {
    return m_updaters;
  }
  bool containsUpdater(QWidget *widget) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto updater: m_updaters )
      if ( updater->widget() == widget )
        return true;
    return false;
  }
  QStyleUpdater *updater(QWidget *widget) const
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto updater: m_updaters )
      if ( updater->widget() == widget )
        return updater;
    return nullptr;
  }

  bool autoReloadStyle() const
  {
    return m_autoReload;
  }
public slots:
  void add(Item::Type type, const QString &path)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    if ( type == Item::Type::File )
      addFile( path );
    else
      addDirectory( path );
  }
  void addFile(const QString &path)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    if ( !containsFile( path ) ) {
      m_items << Item { Item::Type::File, path };
      m_guards[ path ] = new QStyleLoaderFileGuard( QDir::fromNativeSeparators( path ),
                                                    this,
                                                    this );
      reloadAllStylePrivate();
    }
  }
  void addDirectory(const QString &path)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    if ( !containsDirectory( path ) ) {
      m_items << Item { Item::Type::Directory, path };
      m_guards[ path ] = new QStyleLoaderDirectoryGuard( QDir::fromNativeSeparators( path ),
                                                         m_filter,
                                                         this,
                                                         this );
      reloadAllStylePrivate();
    }
  }
  void remove(const QString &path)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    if ( m_guards.contains( path ) ) {
      auto obj = m_guards[ path ];
      m_guards.remove( path );
      delete obj;
    }

    for ( auto &item: m_items ) {
      if ( item.path == path ) {
        m_items.removeOne( item );
        break;
      }
    }
  }

  QStyleUpdater *addUpdater(QWidget *widget)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    return insertUpdater( widget );
  }
  void removeUpdater(QWidget *widget)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto updater: m_updaters ) {
      if ( updater->widget() == widget ) {
        auto u = updater;
        m_updaters.removeOne( updater );
        delete u;
      }
    }
  }
  QStyleUpdater *insertUpdater(QWidget *widget)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    for ( auto updater: m_updaters )
      if ( updater->widget() == widget )
        return updater;

    auto updater = new QStyleUpdater( widget, this );
    connect( updater, &QStyleUpdater::styleReloaded, this, &_QStyleLoader::updaterStyleReloaded );
    m_updaters << updater;
    return updater;
  }

  void reloadAllStyle()
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_hasReload = false;
    m_lastReloaded = QDateTime::currentDateTime();
    QStringList items;

    for ( auto item: m_items ) {
      auto data = loadItem( item );
      if ( !data.isEmpty() )
        items << data;
    }

    qApp->setStyleSheet( items.join( '\n' ) );
  }
  void setAutoReloadStyle(bool enable)
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    m_autoReload = enable;
  }

private slots:
  void reloadAllStylePrivate()
  {
    std::lock_guard<std::recursive_mutex> locker( m_locker );
    if ( autoReloadStyle() ) {
      if ( m_lastReloaded.isNull() || m_lastReloaded.msecsTo( QDateTime::currentDateTime() ) > 2000 ) {
        reloadAllStyle();
      } else {
        m_hasReload = true;
      }
    }
  }

  void updaterStyleReloaded(QWidget *widget)
  {
    auto updater = qobject_cast<QStyleUpdater*>( sender() );
    if ( !!updater )
      emit m_root->styleReloaded( updater, widget );
  }

  QString loadItem(const Item &item)
  {
    if ( item.type == Item::Type::File )
      return loadFile( item.path );
    return loadDirectory( item.path );
  }

  QString loadFile(const QString &path)
  {
    QString result;
    QFile f ( QDir::fromNativeSeparators( path ) );
    if ( f.open( QIODevice::ReadOnly ) ) {
      result = QString::fromUtf8( f.readAll() );
      f.close();
    } else {
      qDebug() << f.errorString();
    }

    return result;
  }

  QString loadDirectory(const QString &path)
  {
    QStringList result;
    QDir directory ( path );

    for ( auto &f: directory.entryInfoList( m_filter, QDir::Filter::Files ) ) {
      auto data = loadFile( f.absoluteFilePath() );
      if ( !data.isEmpty() )
        result << data;
    }

    for ( auto &name: directory.entryList( m_filter, QDir::Filter::Dirs ) ) {
      if ( name == ".." || name == "." ) continue;
      auto data = loadDirectory( directory.absoluteFilePath( name ) );
      if ( !data.isEmpty() )
        result << data;
    }

    return result.join( '\n' );
  }

  // QStyleLoaderGuardObserver
public:
  virtual void added(const QString &path) override
  {
    m_root->fileStyleChanged( path );
    reloadAllStylePrivate();
  }
  virtual void removed(const QString &path) override
  {
    m_root->fileStyleChanged( path );
    reloadAllStylePrivate();
  }
  virtual void changed(const QString &path) override
  {
    m_root->fileStyleChanged( path );
    reloadAllStylePrivate();
  }

  // QObject interface
protected:
  void timerEvent(QTimerEvent *event) override
  {
    reloadAllStylePrivate();
    QObject::timerEvent( event );
  }

};

QStyleLoader::Item::Item(QStyleLoader::Item::Type type, const QString &path)
  : type( type )
  , path( path )
{

}

QStyleLoader::Item::~Item()
{

}

bool QStyleLoader::Item::operator==(const Item &item) const
{
  return type == item.type && path == item.path;
}

bool QStyleLoader::Item::operator!=(const Item &item) const
{
  return type != item.type || path != item.path;
}

QStyleLoader::QStyleLoader(QObject *parent)
  : QObject( parent )
  , ptr ( new _QStyleLoader( this ) )
{
}

QStyleLoader::~QStyleLoader()
{
  delete ptr;
}

QStyleLoader *QStyleLoader::instance()
{
  static QStyleLoader *instancePtr = new QStyleLoader();
  return instancePtr;
}

int QStyleLoader::count() const
{
  return ptr->count();
}

QStyleLoader::Item QStyleLoader::at(int index) const
{
  return ptr->at( index );
}

QList<QStyleLoader::Item> QStyleLoader::items() const
{
  return ptr->items();
}

QStringList QStyleLoader::fileFilters() const
{
  return ptr->fileFilters();
}

bool QStyleLoader::contains(const QString &path) const
{
  return ptr->contains( path );
}

bool QStyleLoader::containsFile(const QString &path) const
{
  return ptr->containsFile( path );
}

bool QStyleLoader::containsDirectory(const QString &path) const
{
  return ptr->containsDirectory( path );
}

QList<QStyleUpdater *> QStyleLoader::updaters() const
{
  return ptr->updaters();
}

bool QStyleLoader::containsUpdater(QWidget *widget) const
{
  return ptr->containsUpdater( widget );
}

QStyleUpdater *QStyleLoader::updater(QWidget *widget) const
{
  return ptr->updater( widget );
}

bool QStyleLoader::autoReloadStyle() const
{
  return ptr->autoReloadStyle();
}

void QStyleLoader::add(QStyleLoader::Item::Type type, const QString &path)
{
  ptr->add( type, path );
}

void QStyleLoader::addFile(const QString &path)
{
  ptr->addFile( path );
}

void QStyleLoader::addDirectory(const QString &path)
{
  ptr->addDirectory( path );
}

void QStyleLoader::remove(const QString &path)
{
  ptr->remove( path );
}

void QStyleLoader::removeUpdater(QWidget *widget)
{
  ptr->removeUpdater( widget );
}

QStyleUpdater *QStyleLoader::addUpdater(QWidget *widget)
{
  return ptr->addUpdater( widget );
}

QStyleUpdater *QStyleLoader::insertUpdater(QWidget *widget)
{
  return ptr->insertUpdater( widget );
}

void QStyleLoader::reloadAllStyle()
{
  ptr->reloadAllStyle();
}

void QStyleLoader::setAutoReloadStyle(bool enable)
{
  ptr->setAutoReloadStyle( enable );
}

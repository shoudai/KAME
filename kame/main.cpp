/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitagawa@scphys.kyoto-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "support.h"

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kapp.h>

#include "kame.h"
#include "xsignal.h"
#include "icons/icon.h"
#include <kiconloader.h>
#include <kstandarddirs.h>
#include <qgl.h>
#include <qfile.h>
#include <qtextcodec.h>
#include <errno.h>

#include <ltdl.h>

static const char *description =
I18N_NOOP("KAME");
// INSERT A DESCRIPTION FOR YOUR APPLICATION HERE
	
	
static KCmdLineOptions options[] =
{
    { "logging", I18N_NOOP("log debuging info."), 0 },
    { "mlockall", I18N_NOOP("never cause swapping, perhaps you need 'ulimit -l <MB>'"), 0 },
    { "nomlock", I18N_NOOP("never use mlock"), 0 },
    { "nodr", 0, 0 },
    { "nodirectrender", I18N_NOOP("do not use direct rendering"), 0 },
    { "module <path>", I18N_NOOP("load a specified module"), "" },
    { "+[File]", I18N_NOOP("measurement file to open"), 0 },
    KCmdLineLastOption
    // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int load_module(const char *filename, lt_ptr ) {
	lt_dlhandle handle = lt_dlopenext(filename);
	if(handle)
		fprintf(stderr, "Module %s loaded\n", filename);
	else
		fprintf(stderr, "loading module %s failed %s\n", filename, lt_dlerror());
	return 0;
}

int main(int argc, char *argv[])
{
#ifdef HAVE_LIBGCCPP
	//initialize GC
	GC_INIT();
	// GC_find_leak = 1;
	//GC_dont_gc
#endif  
   
	KAboutData aboutData( PACKAGE, I18N_NOOP("KAME"),
						  VERSION, description, KAboutData::License_GPL,
						  "(c) 2003-2006, ", 0, 0, "");
	aboutData.addAuthor("Kentaro Kitagawa",0, "kitagawa@scphys.kyoto-u.ac.jp");
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

	KApplication *app;
	app = new KApplication;
  
  	QCStringList module_path;
	{
		KGlobal::dirs()->addPrefix(".");
		makeIcons(app->iconLoader());
		{
			KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
                    
			g_bLogDbgPrint = args->isSet("logging");
            
			g_bMLockAlways = args->isSet("mlockall");

			if(g_bMLockAlways) {
				if(( mlockall(MCL_CURRENT | MCL_FUTURE ) == 0)) {
					dbgPrint("MLOCKALL succeeded.");
				}
				else{
					dbgPrint(formatString("MLOCKALL failed errno=%d.", errno));
				}
			}

			g_bUseMLock = args->isSet("mlock");
			if(g_bUseMLock)
				mlock(&aboutData, 4096uL); //reserve stack of main thread.
        
			QGLFormat f;
			f.setDirectRendering(args->isSet("directrender") );
			QGLFormat::setDefaultFormat( f );
            
			//! Use UTF8 conversion from std::string to QString.
			QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8") );
            
			module_path = args->getOptionList("module");
            
			FrmKameMain *form;
			form = new FrmKameMain();
			app->setMainWidget(form);
			//    form->resize(QSize(QApplication::desktop()->width(), QApplication::desktop()->height() - 200 ).expandedTo(form->sizeHint()) );
			//    form->switchToChildframeMode();
			//        form->setToolviewStyle(KMdi::IconOnly);
			form->setToolviewStyle(KMdi::TextAndIcon);
			//    form->setGeometry(0, 0, form->width(), form->height());
			form->show();
            
			if (args->count())
			{
				form->openMes( QFile::decodeName( args->arg(0)));    
			}
			else
			{
			}
			args->clear();
		}
	}
	/*
	  while(!app->closingDown()) {
	  bool idle = g_signalBuffer->synchronize();  
	  if(idle) app->processEvents();
	  app->processEvents(15);
	  }
	  return 0;
	*/

	lt_dlinit();
	QStringList libdirs = KGlobal::instance()->dirs()->resourceDirs("lib");
	for(QStringList::iterator it = libdirs.begin(); it != libdirs.end(); it++) {
		QString path = *it + "kame/modules";
		lt_dladdsearchdir(path);
		fprintf(stderr, "search in %s\n", (const char*)path);
		lt_dlforeachfile(path, &load_module, NULL);
	}
	for(QCStringList::iterator it = module_path.begin(); it != module_path.end(); it++) {
		load_module(*it, NULL);
	}

	int ret = app->exec();

	return ret;
}  

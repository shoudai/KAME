/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
//---------------------------------------------------------------------------
#include "nodebrowser.h"
#include "nodebrowserform.h"
#include "measure.h"
#include <qlineedit.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qtextbrowser.h>
#include <kapp.h>
#include <klocale.h>

XNodeBrowser::XNodeBrowser
(const shared_ptr<XNode> &root, FrmNodeBrowser *form)
	: XQConnector(root, form),
	m_root(root),
	m_pForm(form),
	m_desc(createOrphan<XStringNode>("Desc", true)),
	m_conDesc(xqcon_create<XQTextBrowserConnector>(m_desc, form->m_txtDesc))
{
	m_pTimer = new QTimer(this);
	connect(m_pTimer, SIGNAL (timeout() ), this, SLOT(process()));
	m_pTimer->start(500);
	form->m_txtDesc->setTextFormat(Qt::LogText);
}

XNodeBrowser::~XNodeBrowser()
{
	m_pTimer->stop();
}
shared_ptr<XNode>
XNodeBrowser::connectedNode(QWidget *widget) {
	if(!widget || (widget == m_pForm->m_txtDesc) ||
			(widget == m_pForm->m_edValue) || (widget == m_pForm)) {
		return shared_ptr<XNode>();
	}
	return XQConnector::connectedNode(widget);
}

void
XNodeBrowser::process() {
	QWidget *widget;
	shared_ptr<XNode> node;
//	widget = KApplication::kApplication()->focusWidget();
//		node = connectedNode(widget);
	if(!node) {
		widget = KApplication::widgetAt(QCursor::pos(), true);
		node = connectedNode(widget);
	}
	if(!node && widget) {
		widget = widget->parentWidget();
		node = connectedNode(widget);
	}

	if(!node)
		node = m_lastPointed;
	if((node != m_lastPointed) && node) {
		shared_ptr<XValueNodeBase> valuenode(dynamic_pointer_cast<XValueNodeBase>(node));
		shared_ptr<XListNodeBase> listnode(dynamic_pointer_cast<XListNodeBase>(node));

		m_conValue.reset();
		if(valuenode)
			m_conValue = xqcon_create<XQLineEditConnector>(valuenode, m_pForm->m_edValue);
		QString str;
		str += "<font color=#005500>Label:</font> ";
		str += node->getLabel();
//		str += "\nName: ";
//		str += node->getName();
		str += "\n";
		if(!node->isUIEnabled()) str+= "UI/scripting disabled.\n";
		if(node->isRunTime()) str+= "For run-time only.\n";
		str += "<font color=#005500>Type:</font> ";
		str += node->getTypename();
		str += "\n";
		std::string rbpath;
		shared_ptr<XNode> cnode = node;
		while(cnode) {
			if((rbpath.length() > 64) ||
					(cnode == m_root.lock())) {
				str += "<font color=#550000>Ruby object:</font>\n Measurement" + rbpath;
				str += "\n<font color=#550000>Supported Ruby methods:</font> \n"
					" name() touch() child(<font color=#000088>name/idx</font>)"
					" [](<font color=#000088>name/idx</font>) count() each() to_ary()";
				if(valuenode)
					str += " set(<font color=#000088>x</font>)"
						" value=<font color=#000088>x</font> load(<font color=#000088>x</font>)"
						" &lt;&lt;<font color=#000088>x</font> get() value() to_str()";
				if(listnode)
					str += " create(<font color=#000088>type</font>, <font color=#000088>name</font>)"
						" release()";
				str += "\n";
				break;
			}
			rbpath = formatString("[\"%s\"]%s", cnode->getName().c_str(), rbpath.c_str());
			cnode = cnode->getParent();
		}
		if(!cnode) {
//			str += rbpath;
			str += "Inaccessible from the root object.\n";		
		}
		atomic_shared_ptr<const XNode::NodeList> list(node->children());
		if(list) { 
			str += formatString("<font color=#005500>%u Child(ren):</font> \n", (unsigned int)list->size());
			for(XNode::NodeList::const_iterator it = list->begin(); it != list->end(); it++) {
				str += " ";
				str += (*it)->getName();
			}
			str += "\n";
		}
		m_desc->str(str);
	}
	m_lastPointed = node;
}

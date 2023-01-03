/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2008 Valentin Rusu kde at rusu.info
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef GUIUTILS_H
#define GUIUTILS_H

#include "defmac.h"
#include <QObject>
#include <kactioncollection.h>

namespace GuiUtils {
   //Use std::enable_if since compilers don't disabiguate overloads based on return type alone
   template <class T, class Receiver, class Func>
      inline typename std::enable_if<std::is_same<T, QAction>::value, QAction>::type* createAction(
       const QString& text,
       const Receiver receiver,
       const Func slot,
       KActionCollection* ac,
       const QString& actionName)
   {
       assert(ac != nullptr);
       QAction* theAction;

       theAction = ac->addAction(actionName);
       theAction->setText(text);
       chk_connect(theAction, &QAction::triggered, receiver, slot);
       return theAction;
   }

   template <class T, class Receiver, class Func>
   inline typename std::enable_if<std::is_same<T, KToggleAction>::value, KToggleAction>::type* createAction(
                   const QString& text,
                   const Receiver receiver,
                   const Func slot,
                   KActionCollection* ac,
                   const QString &actionName)    {
      assert( ac != nullptr );
      KToggleAction* theAction = new KToggleAction(ac);
      ac->addAction( actionName, theAction );
      theAction->setText( text );
      chk_connect( theAction, &KToggleAction::triggered, receiver, slot );
      return theAction;
   }

   template <class T, class Receiver, class Func>
   T* createAction(
     const QString& text,
     const QKeySequence& shortcut,
     Receiver receiver,
     Func slot,
     KActionCollection* ac,
     const QString &actionName)
   {
      T* theAction = createAction<T, Receiver, Func>( text, receiver, slot, ac, actionName );
      ac->setDefaultShortcut(theAction, shortcut);
      return theAction;
   }
   template <class T, class Receiver, class Func>
   T* createAction(
      const QString& text,
      const QIcon& icon,
      Receiver receiver,
      Func slot,
      KActionCollection* ac,
      const QString &actionName)
   {
      T* theAction = createAction<T, Receiver, Func>( text, receiver, slot, ac, actionName );
      theAction->setIcon( icon );
      return theAction;
   }
   template <class T, class Receiver, class Func>
   T* createAction(
      const QString& text,
      const QIcon& icon,
      const QString& iconText,
      Receiver receiver,
      Func slot,
      KActionCollection* ac,
      const QString &actionName)
   {
      T* theAction = createAction<T, Receiver, Func>( text, receiver, slot, ac, actionName );
      theAction->setIcon( icon );
      theAction->setIconText( iconText );
      return theAction;
   }
   template <class T, class Receiver, class Func>
   T* createAction(
     const QString& text,
     const QIcon& icon,
     const QKeySequence& shortcut,
     Receiver receiver,
     Func slot,
     KActionCollection* ac,
     const QString &actionName)
   {
      T* theAction = createAction<T, Receiver, Func>( text, shortcut, receiver, slot, ac, actionName );
      theAction->setIcon( icon );
      return theAction;
   }
   template <class T, class Receiver, class Func>
   T* createAction(
         const QString& text,
         const QIcon& icon,
         const QString& iconText,
         const QKeySequence& shortcut,
         Receiver receiver,
         Func slot,
         KActionCollection* ac,
         const QString &actionName)
   {
      T* theAction = createAction<T, Receiver, Func>( text, shortcut, receiver, slot, ac, actionName );
      theAction->setIcon( icon );
      theAction->setIconText( iconText );
      return theAction;
   }


   //Allow actions to be created without connecting them immediately.

   template <class T>
      inline typename std::enable_if<std::is_same<T, QAction>::value, QAction>::type* createAction(
       const QString& text,
       KActionCollection* ac,
       const QString& actionName)
   {
       assert(ac != nullptr);
       QAction* theAction;

       theAction = ac->addAction(actionName);
       theAction->setText(text);

       return theAction;
   }

   template <class T>
   inline typename std::enable_if<std::is_same<T, KToggleAction>::value, KToggleAction>::type* createAction(
                   const QString& text,
                   KActionCollection* ac,
                   const QString &actionName)    {
      assert( ac != nullptr );
      KToggleAction* theAction = new KToggleAction(ac);
      ac->addAction( actionName, theAction );
      theAction->setText( text );
      return theAction;
   }

   template <class T>
   T* createAction(
     const QString& text,
     const QKeySequence& shortcut,
     KActionCollection* ac,
     const QString &actionName)
   {
      T* theAction = createAction<T>( text, ac, actionName );
      ac->setDefaultShortcut(theAction, shortcut);
      return theAction;
   }
   template <class T>
   T* createAction(
      const QString& text,
      const QIcon& icon,
      KActionCollection* ac,
      const QString &actionName)
   {
      T* theAction = createAction<T>( text, ac, actionName );
      theAction->setIcon( icon );
      return theAction;
   }
   template <class T>
   T* createAction(
      const QString& text,
      const QIcon& icon,
      const QString& iconText,
      KActionCollection* ac,
      const QString &actionName)
   {
      T* theAction = createAction<T>( text, ac, actionName );
      theAction->setIcon( icon );
      theAction->setIconText( iconText );
      return theAction;
   }
   template <class T>
   T* createAction(
     const QString& text,
     const QIcon& icon,
     const QKeySequence& shortcut,
     KActionCollection* ac,
     const QString &actionName)
   {
      T* theAction = createAction<T>( text, shortcut, ac, actionName );
      theAction->setIcon( icon );
      return theAction;
   }
   template <class T>
   T* createAction(
         const QString& text,
         const QIcon& icon,
         const QString& iconText,
         const QKeySequence& shortcut,
         KActionCollection* ac,
         const QString &actionName)
   {
      T* theAction = createAction<T>( text, shortcut, ac, actionName );
      theAction->setIcon( icon );
      theAction->setIconText( iconText );
      return theAction;
   }
}

#endif

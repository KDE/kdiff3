/**
 * Copyright (c) 2008 by Valentin Rusu kde at rusu.info
 * Copyright (C) 2018 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of KDiff3.
 *
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GUIUTILS_H
#define GUIUTILS_H

#include <QObject>
#include <kactioncollection.h>

namespace GuiUtils {
   //Use std::enable_if since complires don't disabiguate overloads based on return type alone
   template <class T, class Receiver, class Func>
      inline typename std::enable_if<std::is_same<T, QAction>::value, QAction>::type* createAction(
       const QString& text,
       const Receiver receiver,
       const Func slot,
       KActionCollection* ac,
       const QString& actionName)
   {
       Q_ASSERT(ac != nullptr);
       QAction* theAction;

       theAction = ac->addAction(actionName);
       theAction->setText(text);
       QObject::connect(theAction, &QAction::triggered, receiver, slot);
       return theAction;
   }

   template <class T, class Receiver, class Func>
   inline typename std::enable_if<std::is_same<T, KToggleAction>::value, KToggleAction>::type* createAction(
                   const QString& text,
                   const Receiver receiver,
                   const Func slot,
                   KActionCollection* ac,
                   const QString &actionName)    {
      Q_ASSERT( ac != nullptr );
      KToggleAction* theAction = new KToggleAction(ac);
      ac->addAction( actionName, theAction );
      theAction->setText( text );
      QObject::connect( theAction, &KToggleAction::triggered, receiver, slot );
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
       Q_ASSERT(ac != nullptr);
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
      Q_ASSERT( ac != nullptr );
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

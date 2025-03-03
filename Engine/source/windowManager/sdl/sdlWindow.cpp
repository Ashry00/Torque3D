//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "math/mMath.h"
#include "gfx/gfxStructs.h"

#include "windowManager/sdl/sdlWindow.h"
#include "windowManager/sdl/sdlWindowMgr.h"
#include "windowManager/sdl/sdlCursorController.h"
#include "platformSDL/sdlInput.h"
#include "platform/platformInput.h"

#include "gfx/gfxDevice.h"

#ifdef TORQUE_OS_LINUX
#define SDL_VIDEO_DRIVER_X11  // TODO SDL
#endif

#include "SDL.h"
#include "SDL_syswm.h"

#define SCREENSAVER_QUERY_DENY 0 // Disable screensaver

#ifndef IDI_ICON1 
#define IDI_ICON1 107
#endif

namespace 
{
   U32 getTorqueModFromSDL(U16 mod)
   {
      U32 ret = 0;

      if (mod & KMOD_LSHIFT)
      {
         ret |= SI_LSHIFT;
         ret |= SI_SHIFT;
      }

      if (mod & KMOD_RSHIFT)
      {
         ret |= SI_RSHIFT;
         ret |= SI_SHIFT;
      }

      if (mod & KMOD_LCTRL)
      {
         ret |= SI_LCTRL;
         ret |= SI_CTRL;
      }

      if (mod & KMOD_RCTRL)
      {
         ret |= SI_RCTRL;
         ret |= SI_CTRL;
      }

      if (mod & KMOD_LALT)
      {
         ret |= SI_LALT;
         ret |= SI_ALT;
      }

      if (mod & KMOD_RALT)
      {
         ret |= SI_RALT;
         ret |= SI_ALT;
      }

      return ret;
   }
}

PlatformWindowSDL::PlatformWindowSDL():
mOwningManager(NULL),
mNextWindow(NULL),
mWindowHandle(NULL),
mOldParent(NULL),
mDevice(NULL),
mTarget(NULL),
mPosition(0,0),
mMouseLocked(false),
mShouldLockMouse(false),
mSuppressReset(false),
mMenuHandle(NULL),
mClosing(false)
{
   mCursorController = new PlatformCursorControllerSDL( this );

   mVideoMode.bitDepth = 32;
   mVideoMode.fullScreen = false;
   mVideoMode.refreshRate = 60;
   mVideoMode.resolution.set(800,600);
}

PlatformWindowSDL::~PlatformWindowSDL()
{
   // delete our sdl handle..
   SDL_DestroyWindow(mWindowHandle);

   // unlink ourselves from the window list...
   AssertFatal(mOwningManager, "PlatformWindowSDL::~PlatformWindowSDL - orphan window, cannot unlink!");
   mOwningManager->unlinkWindow(this);
}

GFXDevice * PlatformWindowSDL::getGFXDevice()
{
   return mDevice;
}

GFXWindowTarget * PlatformWindowSDL::getGFXTarget()
{
   return mTarget;
}

const GFXVideoMode & PlatformWindowSDL::getVideoMode()
{
   return mVideoMode;
}

void* PlatformWindowSDL::getSystemWindow(const WindowSystem system)
{
     SDL_SysWMinfo info;
     SDL_VERSION(&info.version);
     SDL_GetWindowWMInfo(mWindowHandle,&info);     

#ifdef TORQUE_OS_WIN
     if( system == WindowSystem_Windows && info.subsystem == SDL_SYSWM_WINDOWS)
        return info.info.win.window;
#endif

#if defined(TORQUE_OS_LINUX)
     if( system == WindowSystem_X11 && info.subsystem == SDL_SYSWM_X11)
        return (void*)info.info.x11.window;
#endif

    AssertFatal(0, "");
    return NULL;
}

void PlatformWindowSDL::_setVideoMode( const GFXVideoMode &mode )
{
   mVideoMode = mode;
   mSuppressReset = true;
   S32 newDisplay = Con::getIntVariable("pref::Video::deviceId", 0);

   // Set our window to have the right style based on the mode
   if(mode.fullScreen && !Platform::getWebDeployment() && !mOffscreenRender)
   {
      SDL_Rect rect_sdl;
      // Move the window onto the correct monitor before setting fullscreen
      if (0 == SDL_GetDisplayBounds(newDisplay, &rect_sdl))
      {
         SDL_SetWindowPosition(mWindowHandle, rect_sdl.x, rect_sdl.y);
      }

      setSize(mode.resolution);

      SDL_SetWindowFullscreen( mWindowHandle, SDL_WINDOW_FULLSCREEN);

      // When switching to Fullscreen, reset device after setting style
      if(mTarget.isValid())
         mTarget->resetMode();
   }
   else
   {
      // Reset device *first*, so that when we call setSize() and let it
      // access the monitor settings, it won't end up with our fullscreen
      // geometry that is just about to change.

      if(mTarget.isValid())
         mTarget->resetMode();

      if (!mOffscreenRender)
      {
         SDL_SetWindowFullscreen( mWindowHandle, 0);
      }

      // Restore the window to it's original size/position before applying changes
      SDL_RestoreWindow(mWindowHandle);

      // pref::Video::deviceMode values 0-windowed, 1-borderless, 2-fullscreen
      bool hasBorder = (0 == Con::getIntVariable("pref::Video::deviceMode", 0));
      SDL_SetWindowBordered(mWindowHandle, hasBorder ? SDL_TRUE : SDL_FALSE);
      setSize(mode.resolution);
      SDL_SetWindowPosition(mWindowHandle, SDL_WINDOWPOS_CENTERED_DISPLAY(newDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(newDisplay));
      if (hasBorder && Con::getBoolVariable("pref::Video::isMaximized", false))
         SDL_MaximizeWindow(mWindowHandle);
   }

   mSuppressReset = false;
}

bool PlatformWindowSDL::clearFullscreen()
{
   return true;
}

bool PlatformWindowSDL::isFullscreen()
{   
   U32 flags = SDL_GetWindowFlags( mWindowHandle );   
   if( flags & SDL_WINDOW_FULLSCREEN || flags & SDL_WINDOW_FULLSCREEN_DESKTOP )
      return true;

   return false;
}

void PlatformWindowSDL::_setFullscreen(const bool fullscreen)
{
   if( isFullscreen() )
      return;

   if(fullscreen && !mOffscreenRender)
   {
      Con::printf("PlatformWindowSDL::setFullscreen (full) enter");
      SDL_SetWindowFullscreen( mWindowHandle, SDL_WINDOW_FULLSCREEN);
   }
   else
   {
      Con::printf("PlatformWindowSDL::setFullscreen (windowed) enter");
      if (!mOffscreenRender)
      {
         SDL_SetWindowFullscreen( mWindowHandle, SDL_WINDOW_FULLSCREEN_DESKTOP);
      }

      setSize(mVideoMode.resolution);

   }
   Con::printf("PlatformWindowSDL::setFullscreen exit");   
}

bool PlatformWindowSDL::setCaption( const char *cap )
{
   SDL_SetWindowTitle(mWindowHandle, cap);
   return true;
}

const char * PlatformWindowSDL::getCaption()
{
   return StringTable->insert( SDL_GetWindowTitle(mWindowHandle) );
}

void PlatformWindowSDL::setFocus()
{
   SDL_RaiseWindow(mWindowHandle);
}

void PlatformWindowSDL::setClientExtent( const Point2I newExtent )
{
   Point2I oldExtent = getClientExtent();
   if (oldExtent == newExtent)
      return;   

   SDL_SetWindowSize(mWindowHandle, newExtent.x, newExtent.y);
}

const Point2I PlatformWindowSDL::getClientExtent()
{
   // Fetch Client Rect from Windows
   Point2I size;
   SDL_GetWindowSize(mWindowHandle, &size.x, &size.y);

   return size;
}

void PlatformWindowSDL::setBounds( const RectI &newBounds )
{
   // TODO SDL
}

const RectI PlatformWindowSDL::getBounds() const
{
   // TODO SDL
   return RectI(0, 0, 0, 0);   
}

void PlatformWindowSDL::setPosition( const Point2I newPosition )
{
   SDL_SetWindowPosition( mWindowHandle, newPosition.x, newPosition.y );
}

const Point2I PlatformWindowSDL::getPosition()
{
   Point2I position;
   SDL_GetWindowPosition( mWindowHandle, &position.x, &position.y );

   // Return position
   return position;
}

Point2I PlatformWindowSDL::clientToScreen( const Point2I& pos )
{
   Point2I position;
   SDL_GetWindowPosition( mWindowHandle, &position.x, &position.y );
   return pos + position;
}

Point2I PlatformWindowSDL::screenToClient( const Point2I& pos )
{
   Point2I position;
   SDL_GetWindowPosition( mWindowHandle, &position.x, &position.y );
   return pos - position;
}

void PlatformWindowSDL::centerWindow()
{
   int sizeX, sizeY;
   SDL_GetWindowSize(mWindowHandle, &sizeX, &sizeY);

   SDL_DisplayMode mode;
   SDL_GetDesktopDisplayMode(0, &mode);
   
   U32 posX = (mode.w/2) - (sizeX/2);
   U32 posY = (mode.h/2) - (sizeY/2);

   SDL_SetWindowPosition( mWindowHandle, posX, posY);
}

bool PlatformWindowSDL::setSize( const Point2I &newSize )
{
   SDL_SetWindowSize(mWindowHandle, newSize.x, newSize.y);
   return true;
}

bool PlatformWindowSDL::isOpen()
{
   return mWindowHandle;
}

bool PlatformWindowSDL::isVisible()
{
   // Is the window open and visible, ie. not minimized?
   if(!mWindowHandle)
      return false;

   if (mOffscreenRender)
      return true;

   U32 flags = SDL_GetWindowFlags( mWindowHandle );   
   if( flags & SDL_WINDOW_SHOWN)
      return true;

   return false;
}

bool PlatformWindowSDL::isFocused()
{
   if (mOffscreenRender)
      return true;

   U32 flags = SDL_GetWindowFlags( mWindowHandle );   
   if( flags & SDL_WINDOW_INPUT_FOCUS || flags & SDL_WINDOW_INPUT_GRABBED || flags & SDL_WINDOW_MOUSE_FOCUS )
      return true;

   return false;
}

bool PlatformWindowSDL::isMinimized()
{
   if (mOffscreenRender)
      return false;

   U32 flags = SDL_GetWindowFlags( mWindowHandle );   
   if( flags & SDL_WINDOW_MINIMIZED)
      return true;

    return false;
}

bool PlatformWindowSDL::isMaximized()
{
   if (mOffscreenRender)
      return true;

   U32 flags = SDL_GetWindowFlags( mWindowHandle );   
   if( flags & SDL_WINDOW_MAXIMIZED)
      return true;

    return false;
}

WindowId PlatformWindowSDL::getWindowId()
{
   return mWindowId;
}

void PlatformWindowSDL::minimize()
{
   if (mOffscreenRender)
      return;

   SDL_MinimizeWindow( mWindowHandle );
}

void PlatformWindowSDL::maximize()
{
   if (mOffscreenRender)
      return;

   SDL_MaximizeWindow( mWindowHandle );
}

void PlatformWindowSDL::restore()
{
   if (mOffscreenRender)
      return;

   SDL_RestoreWindow( mWindowHandle );
}

void PlatformWindowSDL::hide()
{
   if (mOffscreenRender)
      return;

   SDL_HideWindow( mWindowHandle );
}

void PlatformWindowSDL::show()
{
   if (mOffscreenRender)
      return;

   SDL_ShowWindow( mWindowHandle );
}

void PlatformWindowSDL::close()
{
   delete this;
}

void PlatformWindowSDL::defaultRender()
{
   // TODO SDL
}

void PlatformWindowSDL::_triggerMouseLocationNotify(const SDL_Event& evt)
{
   U32 mods = getTorqueModFromSDL(SDL_GetModState());

   if(!mMouseLocked)
      mouseEvent.trigger(getWindowId(), mods, evt.motion.x, evt.motion.y, false);
   else
      mouseEvent.trigger(getWindowId(), mods, evt.motion.xrel, evt.motion.yrel, true);
}

void PlatformWindowSDL::_triggerMouseWheelNotify(const SDL_Event& evt)
{
   U32 mods = getTorqueModFromSDL(SDL_GetModState());
   S32 wheelDelta = Con::getIntVariable("$pref::Input::MouseWheelSpeed", 120);
   wheelEvent.trigger(getWindowId(), mods, evt.wheel.x * wheelDelta, evt.wheel.y * wheelDelta);
}

void PlatformWindowSDL::_triggerMouseButtonNotify(const SDL_Event& event)
{
   S32 action = (event.type == SDL_MOUSEBUTTONDOWN) ? SI_MAKE : SI_BREAK;
   S32 button = -1;

   switch (event.button.button)
   {
      case SDL_BUTTON_LEFT:
         button = 0;
         break;
      case SDL_BUTTON_RIGHT:
         button = 1;
         break;
      case SDL_BUTTON_MIDDLE:
         button = 2;
         break;
      case SDL_BUTTON_X1:
         button = 3;
         break;
      case SDL_BUTTON_X2:
         button = 4;
         break;
      default:
         return;
   }
   
   U32 mod = getTorqueModFromSDL( SDL_GetModState() );
   buttonEvent.trigger(getWindowId(), mod, action, button );
}

void PlatformWindowSDL::_triggerKeyNotify(const SDL_Event& evt)
{
   U32 inputAction = IA_MAKE;
   SDL_Keysym tKey = evt.key.keysym;

   if(evt.type == SDL_KEYUP)
   {
      inputAction = IA_BREAK;
   }

   if(evt.key.repeat)
   {
      inputAction = IA_REPEAT;
   }

   U32 torqueModifiers = getTorqueModFromSDL(evt.key.keysym.mod);
   U32 torqueKey = KeyMapSDL::getTorqueScanCodeFromSDL(tKey.scancode);
   if(tKey.scancode)
   {
      keyEvent.trigger(getWindowId(), torqueModifiers, inputAction, torqueKey);
      //Con::printf("Key %d : %d", tKey.sym, inputAction);

      if (inputAction == IA_MAKE && SDL_IsTextInputActive())
      {
         // We have to check if we already have a first responder active.
         // We don't want to type the character if it actually creates another responder!
         if (mWindowInputGenerator->lastKeyWasGlobalActionMap())
         {
            // Turn off Text input, and the next frame turn it back on. This tells SDL
            // to not generate a text event for this global action map key.
            SDL_StopTextInput();
            mOwningManager->updateSDLTextInputState(PlatformWindowManagerSDL::KeyboardInputState::TEXT_INPUT);
         }
      }
   }
}

void PlatformWindowSDL::_triggerTextNotify(const SDL_Event& evt)
{
    U32 mod = getTorqueModFromSDL( SDL_GetModState() );
   
   if( !evt.text.text[1] ) // get a char
   {
      U16 wchar = evt.text.text[0];
      charEvent.trigger(getWindowId(), mod, wchar );
      //Con::printf("Char: %c", wchar);
      return;
   }
   else // get a wchar string
   {
      const dsize_t len = strlen(evt.text.text);
      U16 wchar[16] = {};
      dMemcpy(wchar, evt.text.text, sizeof(char)*len);

      for(int i = 0; i < 16; ++i)
      {
         if( !wchar[i] )
            return;

         charEvent.trigger(getWindowId(), mod, wchar[i] );
      }
   }
}

void PlatformWindowSDL::_updateMonitorFromMove(const SDL_Event& evt)
{
   SDL_Rect sdlRect;
   S32 monitorCount = SDL_GetNumVideoDisplays();
   for (S32 index = 0; index < monitorCount; ++index)
   {
      if (0 == SDL_GetDisplayBounds(index, &sdlRect))
      {
         if ((evt.window.data1 >= sdlRect.x) && (evt.window.data1 < (sdlRect.x + sdlRect.w)) &&
            (evt.window.data2 >= sdlRect.y) && (evt.window.data2 < (sdlRect.y + sdlRect.h)))
         {
            Con::setIntVariable("pref::Video::deviceId", index);
            return;
         }
      }
   }
}

void PlatformWindowSDL::_processSDLEvent(SDL_Event &evt)
{
   switch(evt.type)
   {        
      case SDL_KEYDOWN:
      case SDL_KEYUP:
      {
         _triggerKeyNotify(evt);
         break;
      }

      case SDL_TEXTINPUT:
      {         
         _triggerTextNotify(evt);
         break;
      }

      case SDL_MOUSEWHEEL:
      {
         _triggerMouseWheelNotify(evt);
         break;
      }

      case SDL_MOUSEMOTION:
      {
         _triggerMouseLocationNotify(evt);
         break;
      }
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
      {
         _triggerMouseButtonNotify(evt);
         break;
      }

      case SDL_WINDOWEVENT:
      {
         if (!mClosing)
         {
            switch (evt.window.event)
            {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
               appEvent.trigger(getWindowId(), GainFocus);
               break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
               appEvent.trigger(getWindowId(), LoseFocus);
               break;
            case SDL_WINDOWEVENT_MOVED:
            {
               _updateMonitorFromMove(evt);
               break;
            }
            case SDL_WINDOWEVENT_RESIZED:
            {
               int width, height;
               SDL_GetWindowSize(mWindowHandle, &width, &height);
               mVideoMode.resolution.set(width, height);
               getGFXTarget()->resetMode();
               resizeEvent.trigger(getWindowId(), width, height);
               getScreenResChangeSignal().trigger(this, true);
               break;
            }
            case SDL_WINDOWEVENT_CLOSE:
            {
               appEvent.trigger(getWindowId(), WindowClose);
               mClosing = true;
            }
            case SDL_WINDOWEVENT_MINIMIZED:
               break;
            case SDL_WINDOWEVENT_MAXIMIZED:
               Con::setBoolVariable("pref::Video::isMaximized", true);
               break;
            case SDL_WINDOWEVENT_RESTORED:
               Con::setBoolVariable("pref::Video::isMaximized", false);
               break;

            default:
               break;
            }
         }
      }
   }

}

//-----------------------------------------------------------------------------
// Mouse Locking
//-----------------------------------------------------------------------------

void PlatformWindowSDL::setMouseLocked( bool enable )
{
   if (mOffscreenRender)
      return;

   mMouseLocked = enable;
   
   SDL_SetWindowGrab( mWindowHandle, SDL_bool(enable) );
   SDL_SetRelativeMouseMode( SDL_bool(enable) );
}

const UTF16 *PlatformWindowSDL::getWindowClassName()
{
   // TODO SDL
   static String str("WindowClassName");
   return str.utf16();
}

const UTF16 *PlatformWindowSDL::getCurtainWindowClassName()
{
   // TODO SDL
   static String str("CurtainWindowClassName");
   return str.utf16();
}

void PlatformWindowSDL::setKeyboardTranslation(const bool enabled)
{
   mEnableKeyboardTranslation = enabled;

   // Flag for update. Let SDL know what kind of input state we are changing to.
   if (enabled)
      mOwningManager->updateSDLTextInputState(PlatformWindowManagerSDL::KeyboardInputState::TEXT_INPUT);
   else
      mOwningManager->updateSDLTextInputState(PlatformWindowManagerSDL::KeyboardInputState::RAW_INPUT);
}

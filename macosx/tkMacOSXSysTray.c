/*
 * tkMacOSXSysTray.c --
 *
 *	tkMacOSXSysTray.c implements a "systray" Tcl command which allows 
 *      one to change the system tray/taskbar icon of a Tk toplevel 
 *      window and a "sysnotify" command to post system notifications.
 *
 * Copyright (c) 2020 Kevin Walzer/WordTech Communications LLC.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <tkInt.h>
#include <tkMacOSXInt.h>
#include "tkMacOSXPrivate.h"

/*
 * Script callback when status icon is clicked.
 */

char * callbackproc;

/*
 * Class declarations and implementations for TkStatusItem.
 */

@interface TkStatusItem: NSObject {
    NSStatusItem * statusItem;
    NSStatusBar * statusBar;
    NSImage * icon;
    NSString * tooltip;
}

- (id) init;
- (void) setImagewithImage : (NSImage * ) image;
- (void) setTextwithString : (NSString * ) string;
- (void) clickOnStatusItem: (id) sender;
- (void) dealloc;


@end

@implementation TkStatusItem : NSObject

- (id) init {
    [super init];
    statusBar = [NSStatusBar systemStatusBar];
    statusItem = [[statusBar statusItemWithLength:NSVariableStatusItemLength] retain];
    statusItem.button.target = self;
    statusItem.button.action = @selector(clickOnStatusItem: );
    statusItem.visible = YES;
    return self;
}

- (void) setImagewithImage : (NSImage * ) image
{
    icon = nil;
    icon = image;
    statusItem.button.image = icon;
}

- (void) setTextwithString : (NSString * ) string
{
    tooltip = nil;
    tooltip = string;
    statusItem.button.toolTip = tooltip;
}

- (void) clickOnStatusItem: (id) sender
{
    if (NSApp.currentEvent.clickCount == 1) {
	TkMainInfo * info = TkGetMainInfoList();
	Tcl_GlobalEval(info -> interp, callbackproc);
    }
}

- (void) dealloc
{
     /*
     * We are only doing the minimal amount of deallocation that
     * the superclass cannot handle when it is deallocated, per 
     * https://developer.apple.com/documentation/objectivec/nsobject/
     * 1571947-dealloc. The compiler may offer warnings, but disregard.
     * Putting too much here can cause unpredictable crashes, especially
     * in the Tk test suite.
     */
    [statusBar removeStatusItem: statusItem];
}

@end

/*
 * Class declarations and implementations for TkNotifyItem.
 */

@interface TkNotifyItem: NSObject {

    NSUserNotification *tk_notification;
    NSString *header;
    NSString *info;
}

- (id) init;
- (void) postNotificationWithTitle : (NSString *) title message: (NSString *) detail;
- (BOOL) userNotificationCenter:(NSUserNotificationCenter *)center
	  shouldPresentNotification:(NSUserNotification *)notification;
- (void) dealloc;


@end

@implementation TkNotifyItem : NSObject

-  (id) init
{
    [super init];
    tk_notification = [[NSUserNotification alloc] init];
    return self;
}

-  (void) postNotificationWithTitle : (NSString * ) title message: (NSString * ) detail
{
    header = title;
    tk_notification.title = header;
    info = detail;
    tk_notification.informativeText = info;
    tk_notification.soundName = NSUserNotificationDefaultSoundName;

    NSUserNotificationCenter *center = [NSUserNotificationCenter defaultUserNotificationCenter];
    
    /*
     * This API requires an app delegate to function correctly. 
     * The compiler may complain that setting TkNotificationItem is an 
     * incompatible type, but disregard. Setting to something else will
     * either cause Wish not to build, or the notification not to display.
     */
    [center setDelegate: self];
    
    [center deliverNotification:tk_notification];
}

- (BOOL) userNotificationCenter: (NSUserNotificationCenter * ) center
      shouldPresentNotification: (NSUserNotification * ) notification
{
    return YES;
}

-  (void) dealloc
{
    /*
     * We are only doing the minimal amount of deallocation that
     * the superclass cannot handle when it is deallocated, per 
     * https://developer.apple.com/documentation/objectivec/nsobject/
     * 1571947-dealloc. The compiler may offer warnings, but disregard.
     * Putting too much here can cause unpredictable crashes, especially
     * in the Tk test suite.
     */
    tk_notification = nil;
}

@end

/*
 * Main objects of this file.
 */
TkStatusItem *tk_item;
TkNotifyItem *notify_item;

/*
 * Forward declarations for procedures defined in this file.
 */

static int
MacSystrayCmd(ClientData clientData, Tcl_Interp * interp,
	      int argc,
	      const char * argv[]);
static void
MacSystrayDestroy();
static void SysNotifyDeleteCmd ( ClientData cd );
static int SysNotifyCmd (ClientData clientData, Tcl_Interp * interp,
			 int argc, const char * argv[]);
int
MacSystrayInit(Tcl_Interp * interp);

/*
 *----------------------------------------------------------------------
 *
 * MacSystrayCmd --
 *
 * 	Main command for creating, displaying, and removing icons from status menu.
 *
 * Results:
 *	Management of icon display in status menu.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */


static int
MacSystrayCmd(ClientData clientData, Tcl_Interp * interp,
	      int argc,
	      const char * argv[]) {

    (void) clientData;
    int length;
    length = strlen(argv[1]);
    if ((strncmp(argv[1], "create", length) == 0) && (length >= 2)) {

	if (argc < 5) {
	    Tcl_AppendResult(interp, " wrong # args: should be \"systray create image ? text? callback?\"", (char * ) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Create the icon.
	 */

	int width, height;
	Tk_Window tkwin = Tk_MainWindow(interp);
	TkWindow *winPtr = (TkWindow *) tkwin;
	Display * d;
	d = winPtr -> display;
	NSImage * icon;

	char * tk_imagename = (char*) argv[2];
	Tk_Image tk_image;
	tk_image = Tk_GetImage(interp, tkwin, tk_imagename, NULL, NULL);
	if (tk_image == NULL) {
	    Tcl_AppendResult(interp, " unable to obtain image for systray icon", (char * ) NULL);
	    return TCL_ERROR;
	}

	Tk_SizeOfImage(tk_image, & width, & height);
	if (width != 0 && height != 0) {
	    icon = TkMacOSXGetNSImageFromTkImage(d, tk_image,
						 width, height);
	    [tk_item setImagewithImage: icon];
	    Tk_FreeImage(tk_image);
	}

	/*
	 * Set the text for the tooltip.
	 */

	NSString * tooltip = [NSString stringWithUTF8String: argv[3]];
	if (tooltip == nil) {
	    Tcl_AppendResult(interp, " unable to set tooltip for systray icon", (char * ) NULL);
	    return TCL_ERROR;
	}

	[tk_item setTextwithString: tooltip];

	/*
	 * Set the proc for the callback.
	 */

	callbackproc = (char*) argv[4];
	if (callbackproc == NULL) {
	    Tcl_AppendResult(interp, " unable to get the callback for systray icon", (char * ) NULL);
	    return TCL_ERROR;
	}

    } else if ((strncmp(argv[1], "modify",  length) == 0) &&
	       (length >= 2)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"systray modify object item?\"", (char * ) NULL);
	    return TCL_ERROR;
	}

	char * modifyitem = (char*) argv[2];

	/*
	 * Modify the icon.
	 */

	if (strcmp (modifyitem, "image") == 0) {

	    Tk_Window tkwin = Tk_MainWindow(interp);
	    TkWindow *winPtr = (TkWindow*) tkwin;
	    Display * d;
	    d = winPtr -> display;
	    NSImage * icon;
	    int width, height;

	    char * tk_imagename = (char*) argv[3];
	    Tk_Image tk_image;
	    tk_image = Tk_GetImage(interp, tkwin, tk_imagename, NULL, NULL);
	    if (tk_image == NULL) {
		Tcl_AppendResult(interp, " unable to obtain image for systray icon", (char * ) NULL);
		return TCL_ERROR;
	    }

	    Tk_SizeOfImage(tk_image, & width, & height);
	    if (width != 0 && height != 0) {
		icon = TkMacOSXGetNSImageFromTkImage(d, tk_image,
						     width, height);
		[tk_item setImagewithImage: icon];
	    }
	    Tk_FreeImage(tk_image);
	}

	/*
	 * Modify the text for the tooltip.
	 */

	if (strcmp (modifyitem, "text") == 0) {

	    NSString * tooltip = [NSString stringWithUTF8String:argv[3]];
	    if (tooltip == nil) {
		Tcl_AppendResult(interp, " unable to set tooltip for systray icon", (char * ) NULL);
		return TCL_ERROR;
	    }

	    [tk_item setTextwithString: tooltip];
	}

	/*
	 * Modify the proc for the callback.
	 */

	if (strcmp (modifyitem, "callback") == 0) {
	    callbackproc = (char*) argv[3];
	    if (callbackproc == NULL) {
		Tcl_AppendResult(interp, " unable to get the callback for systray icon", (char * ) NULL);
		return TCL_ERROR;
	    }
	}

    } else if ((strncmp(argv[1], "destroy", length) == 0) && (length >= 2)) {
	[tk_item dealloc];
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MacSystrayDestroy --
 *
 * 	Deletes icon from display.
 *
 * Results:
 *	Icon/window removed and memory freed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
MacSystrayDestroy() {

    [tk_item dealloc];

}


/*
 *----------------------------------------------------------------------
 *
 * SysNotifyDeleteCmd --
 *
 *      Delete notification and clean up.
 *
 * Results:
 *	Window destroyed.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------z---------------------------------------
 */


static void SysNotifyDeleteCmd ( ClientData cd )
{
    (void) cd;
    [notify_item dealloc];
}


/*
 *----------------------------------------------------------------------
 *
 * SysNotifyCreateCmd --
 *
 *      Create tray command and (unreal) window.
 *
 * Results:
 *	Icon tray and hidden window created.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------z---------------------------------------
 */


static int SysNotifyCmd(ClientData clientData, Tcl_Interp * interp,
			int argc, const char * argv[])
{
    (void)clientData;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args,must be:",
			 argv[0], " title  message ", (char * ) NULL);
	return TCL_ERROR;
    }

    NSString *title = [NSString stringWithUTF8String: argv[1]];
    NSString *message = [NSString stringWithUTF8String: argv[2]];
    [notify_item postNotificationWithTitle : title message: message];

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MacSystrayInit --
 *
 * 	Initialize this package and create script-level commands.
 *
 * Results:
 *	Initialization of code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
MacSystrayInit(Tcl_Interp * interp) {

    /*
     * Initialize TkStatusItem and TkNotifyItem.
     */

    tk_item = [[TkStatusItem alloc] init];
    notify_item = [[TkNotifyItem alloc] init];

    if ([NSApp macOSVersion] < 101000) {
	Tcl_AppendResult(interp, "Statusitem icons not supported on versions of macOS lower than 10.10",  (char * ) NULL);
	return TCL_OK;
    }

    Tcl_CreateCommand(interp, "_systray", MacSystrayCmd, (ClientData)interp,
		      (Tcl_CmdDeleteProc *) MacSystrayDestroy);
    Tcl_CreateCommand(interp, "_sysnotify", SysNotifyCmd, NULL, (Tcl_CmdDeleteProc *) SysNotifyDeleteCmd);

    return TCL_OK;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
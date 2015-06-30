#!/usr/bin/python
import os
import sys
import pygtk
import gtk

# constant parameters
DIMENSION = 32
TILES_ACROSS = 16
TILES_DOWN = 10
TILE_PADDING = 2
WINDOW_PADDING = 32
TILEDIR = ".tiles"
TITLEBAR = "TileSet Editor - pokgame"

# widgets
win = gtk.Window(gtk.WINDOW_TOPLEVEL)
box = gtk.HBox() # pack the vertical scrollbar and draw area into this box
vscroll = gtk.VScrollbar()
drawArea = gtk.DrawingArea()

# drawing info
gc = None
backBuf = None
numRows = 0
index = None
images = None
files = None

# tile swapping info
selTile = -1
madeChanges = False

def insert(src,dst):
    global index
    global madeChanges

    if src == dst:
        return

    tmp = index[src]
    if src > dst:
        for j in reversed(range(dst,src)):
            index[j+1] = index[j]
    else:
        dst -= 1
        for j in range(src,dst):
            index[j] = index[j+1]
    index[dst] = tmp
    madeChanges = True

def swap(src,dst):
    if src == dst:
        return

    tmp = index[src]
    index[src] = index[dst]
    index[dst] = tmp
    madeChanges = True

def tile_file_compar(a,b):
    if a[0].isalpha() and b[0].isalpha():
        return int(a[1:]) - int(b[1:])
    elif a[0].isalpha():
        return 1
    elif b[0].isalpha():
        return -1
    return int(a) - int(b)

def load_tile_list():
    global numRows
    global index
    global images
    global files
    global selTile

    files = os.listdir(".")
    files.sort(cmp=tile_file_compar)
    images = []
    index = []
    i = 0
    for f in files:
        fd = open(f)
        images.append( fd.read() )
        index.append(i)
        i += 1
        fd.close()
    numRows = i / 16 + 1
    selTile = -1

def save_changes():
    global selTile
    global madeChanges

    # use an insertion sort technique to rename tile files in the correct order
    for i in range(1,len(index)):
        if tile_file_compar(files[index[i-1]],files[index[i]]) > 0:
            j = i
            while j > 0 and tile_file_compar(files[index[j-1]],files[index[j]]) > 0:
                os.rename( files[index[j-1]], ".t" )
                os.rename( files[index[j]], files[index[j-1]] )
                os.rename( ".t", files[index[j]] )
                tmp = index[j-1]
                index[j-1] = index[j]
                index[j] = tmp
                j -= 1
    selTile = -1
    madeChanges = False

def set_scroll_adjust():
    # create adjustment
    rng = numRows - TILES_DOWN
    if rng < 0:
        rng = 0
    if not vscroll.get_adjustment is None:
        oldValue = vscroll.get_adjustment().get_value()
    adj = gtk.Adjustment(oldValue,0,rng,page_incr=5)
    adj.connect("value-changed",on_scroll)
    vscroll.set_adjustment(adj)

def load_widgets():
    width = TILES_ACROSS * DIMENSION + (TILES_ACROSS - 1) * TILE_PADDING
    height = TILES_DOWN * DIMENSION + (TILES_DOWN - 1) * TILE_PADDING

    set_scroll_adjust()

    # prepare drawing area; pack it and the scroll bar into the box
    drawArea.add_events(gtk.gdk.BUTTON_RELEASE_MASK | gtk.gdk.BUTTON_PRESS_MASK)
    drawArea.connect("configure-event",on_configure_draw)
    drawArea.connect("expose-event",on_expose)
    drawArea.connect("button-release-event",on_click)
    drawArea.set_size_request(width,height)
    box.add(drawArea)
    box.add(vscroll)
    
    # setup main window
    width += WINDOW_PADDING * 2
    height += WINDOW_PADDING * 2
    win.connect("delete-event",on_delete)
    win.connect("key-release-event",on_key_release)
    win.set_title(TITLEBAR)
    win.set_border_width(WINDOW_PADDING)
    win.set_geometry_hints(win,width,height,width,height,width,height,-1,-1,-1,-1)
    win.add(box)

def error_box(msg):
    dia = gtk.MessageDialog(win,gtk.DIALOG_MODAL,gtk.MESSAGE_ERROR,gtk.BUTTONS_OK)
    dia.set_markup(msg)
    dia.run()
    dia.destroy()

def on_click(widget,event):
    global selTile

    if event.button != 1 and event.button != 3:
        return

    # compute dimensionalized coordinates of clicked tile; make
    # sure to factor in scroll amount
    x = int( (event.x + TILE_PADDING) / (DIMENSION + TILE_PADDING) )
    y = int( (event.y + TILE_PADDING) / (DIMENSION + TILE_PADDING) ) + int(vscroll.get_adjustment().value)
    t = y * TILES_ACROSS + x

    if t >= len(index):
        selTile = -1
        widget.queue_draw()
        win.set_title(TITLEBAR)
        return

    if selTile == -1:
        selTile = t
        win.set_title(TITLEBAR + " (tile " + str(selTile+1) + ")")
    else:
        if files[index[selTile]][0].isalpha() and not files[index[t]][0].isalpha():
            error_box("Cannot move passable tile to impassable region")
        elif not files[index[selTile]][0].isalpha() and files[index[t]][0].isalpha():
            error_box("Cannot move impassable tile to passable region")
        else:
            if event.button == 1:
                insert(selTile,t)
            else:
                swap(selTile,t)
        selTile = -1
        win.set_title(TITLEBAR)

    widget.queue_draw()

def on_key_release(widget,event):
    global madeChanges

    if event.string == "r" or event.string == "R":
        if madeChanges:
            dialog = gtk.MessageDialog(win,gtk.DIALOG_MODAL,gtk.MESSAGE_QUESTION,gtk.BUTTONS_YES_NO)
            dialog.set_markup("You have made changes that will be forgotten. Continue anyway?")
            res = dialog.run()
            dialog.destroy()
            if res != gtk.RESPONSE_YES:
                return
        load_tile_list()
        set_scroll_adjust() # more tiles could have been loaded (or fewer)
        madeChanges = False
        widget.queue_draw()
    elif event.string == "s" or event.string == "S":
        if madeChanges:
            dialog = gtk.MessageDialog(win,gtk.DIALOG_MODAL,gtk.MESSAGE_QUESTION,gtk.BUTTONS_YES_NO)
            dialog.set_markup("Do you really want to save your changes?")
            res = dialog.run()
            dialog.destroy()
            if res != gtk.RESPONSE_YES:
                return
            save_changes()

def on_configure_draw(widget,event):
    # configure the back buffer; this should only happen once
    global backBuf

    _, _, width, height = widget.get_allocation()
    backBuf = gtk.gdk.Pixmap(widget.window,width,height)

def on_expose(widget,event):
    # draw tiles to the back buffer
    _, _, width, height = widget.get_allocation()
    backBuf.draw_rectangle(widget.get_style().black_gc,True,0,0,width,height)
    it = int(vscroll.get_adjustment().value) * TILES_ACROSS
    for y in range(TILES_DOWN):
        for x in range(TILES_ACROSS):
            if it >= len(index):
                break
            if it == selTile:
                backBuf.draw_rectangle(widget.get_style().white_gc,True,
                                       x * DIMENSION + (x-2) * TILE_PADDING,
                                       y * DIMENSION + (y-2) * TILE_PADDING,
                                       DIMENSION + TILE_PADDING*2,
                                       DIMENSION + TILE_PADDING*2)
            backBuf.draw_rgb_image(widget.get_style().fg_gc[gtk.STATE_NORMAL],
                                   x * DIMENSION + (x-1) * TILE_PADDING,
                                   y * DIMENSION + (y-1) * TILE_PADDING,
                                   DIMENSION,
                                   DIMENSION,
                                   gtk.gdk.RGB_DITHER_NONE,
                                   images[ index[it] ])
            it += 1
        else:
            continue
        break

    # expose the back buffer to the GtkDrawingArea
    x, y, width, height = event.area
    widget.window.draw_drawable(widget.get_style().fg_gc[gtk.STATE_NORMAL],backBuf,x,y,x,y,width,height)

def on_scroll(adj):
    win.queue_draw()
    pass

def on_delete(widget,event):
    if madeChanges:
        # ask the user if they want to save changes
        dialog = gtk.MessageDialog(win,gtk.DIALOG_MODAL,gtk.MESSAGE_QUESTION,gtk.BUTTONS_YES_NO)
        dialog.set_markup("Do you want to save your changes?")
        res = dialog.run()
        dialog.destroy()
        if res == gtk.RESPONSE_YES:
            save_changes()
        elif res != gtk.RESPONSE_NO:
            return True

    gtk.main_quit()
    return False

os.chdir(TILEDIR)
# do init
load_tile_list()
load_widgets()

# show windows; sit in event loop
win.show_all()
gtk.main()

/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "emulation/player.h"

class MainWindow
{
public:
    static const int version = 1;

    template <typename... Args>
    static void displayMessage(Args&&...)
    {}
};

#endif // MAINWINDOW_H

#!/usr/bin/env python3

import time
from menuscript import Script, ActionList, MENU_OBJECT_PATH

al = ActionList(MENU_OBJECT_PATH)
al.appendItem("Menu0", "Menu0")
al.appendItem("Menu1", "Menu1")

t = Script.create(al)
t.run()

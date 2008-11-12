SHEETNODE
A module to host spreadsheets as Drupal nodes.

INSTALLATION
You need the SocialCalc JavaScript spreadsheet first.

To get the latest SocialCalc code, checkout from the SVN repo:
$ svn co https://repo.socialtext.net:8999/svn/socialcalc/trunk

Rename trunk to socialcalc.
$ mv trunk socialcalc

Apply the patch inside the socialcalc folder:
$ cd socialcalc
$ patch -p0 < socialcalctableeditor-absolute.patch


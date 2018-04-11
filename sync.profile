%modules = (
    "QtInstagraph" => "$basedir/src/instagraph",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);
%classnames = (
);
%mastercontent = (
    "core" => "#include <QtCore/QtCore>",
    "network" => "#include <QtNetwork/QtNetwork>",
    "network" => "#include <QtNetwork/QtGui>"
);
%modulepris = (
    "QtInstagraph" => "$basedir/modules/qt_instagraph.pri",
);

%dependencies = (
        "qtbase" => ""
);

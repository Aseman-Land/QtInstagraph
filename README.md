# QtInstagraph
It's instagram API library for Qt/C++ that forked from instagraph app. In fact We converted base classes of the instagraph app to the QtModule :)
It's free, opensource and released under the GPL license.

Forked from: [Instagraph App](https://github.com/turanmahmudov/Instagraph)

# Build

It's easy to build it. install Qt and dependencies:

```bash
sudo apt-get install g++ git qt5-default qtbase5-dev
```

And then build it:

```bash
git clone https://github.com/Aseman-Land/QtInstagraph.git
cd QtInstagraph
mkdir build && cd build
qmake .. -r
make
sudo make install
```

# How to use it

It's easy too. Just add below line to the `.pro` file on your project

```perl
QT += instagraph
```

And include it in the source files:

```c++
#include <QtInstagraph>
```

And Login using below codes:

```C++
Instagraph insta = new Instagraph(this);
insta->setUsername("YOUR_USERNAME");
insta->setPassword("YOUR_PASSWORD");

connect(insta, static_cast<void(Instagraph::*)(QString)>(&Instagraph::error), this, [insta](QString msg){
    qDebug() << msg;
});
connect(insta, &Instagraph::profileConnected, this, [insta](QVariant answer){
    qDebug() << answer;
});
connect(insta, &Instagraph::profileConnectedFail, this, [insta](){
    qDebug() << "profileConnectedFail";
});

insta->login();
```

It's been fun :)
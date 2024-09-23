# vSomeIP

Tuto fait avec vSomeIP 3.4.10, qui est la branche master le lundi 26 août 2024 (commit : 48bc6b8e439371b45bbcfbd5ec94b2e0c37bc2a8).

## Installation

Qu'une seule fois à faire.

### Requirements

Installation des compilateurs g++ et cmake, installation de la librarie nécessaire Boost et installation de la génération de documentation Doxygen

```
sudo apt install build-essential cmake libboost-all-dev asciidoc source-highlight doxygen graphviz
```

Une fois cela fait, vérifier que la version de gcc et g++ est bien la 11.4.0 . La version de boost doit être la 1.74.

### Compiler vsomeip

Téléchargement, création des répertoire de build, compilation et installation de la librairie.

```
git clone https://github.com/COVESA/vsomeip.git
cd vsomeip
mkdir build
cd build
cmake ..
make
make install
```

### Compiler sur yocto (ou avec GCC > 12)

Quand nous compilons sur yocto, la version de gcc utilisée est 13.3. A partir de gcc 12, une erreur  `stringop-overflow` se produit à 54% de la compilation de la librairie vsomeip. Nous pouvons résoudre cette erreur en la transformant en warning et en l'ignorant. L'exécution du code ne changera pas.

Pour cela, modifier le fichier vsomeip/CMakeLists.txt à la ligne 81 (ou 82 suivant les commits). Nous rajoutons ` -Wnoerror=stringop-overflow` à la fin de la ligne afin de convertir les erreurs `stringop-overflow` en warnings.  

```
set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -D_GLIBCXX_USE_NANOSLEEP -pthread -O -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector-strong -fasynchronous-unwind-tables -fno-omit-frame-pointer -D_FORTIFY_SOURCE=${_FORTIFY_SOURCE} -Wformat -Wformat-security -Wpedantic -Werror -fPIE -Wnoerror=stringop-overflow")
```

Ré-exécutez ensuite les commandes précédentes :
```
cmake ..
make
make install
```

### Création de la documentation

Si vous enchaîner les commandes, vous êtes dans le dossier vsomeip/build à ce moment. Sinon, mettez vous-y.

```
make doc
```

## Utilisation de la librairie

Ces commandes sont à utiliser plusieurs fois pendant que vous codez/comprenez la librairie. Chaque bloc de commandes considère que vous êtes dans le dossier racine vsomeip.

### Afficher la doc

Lance firefox et affiche la doc Doxygen créée plus tôt. Si vous faire des modifications dans la librairie elle-même, la doc doit être regénérée avec la commande `make doc` mais si vous modifier juste les exemples, il n'y en a pas besoin.

```
firefox build/documentation/html/index.html
```
Un mini tutoriel de la librairie est créée en un seul fichier html ici : 
```
firefox build/documentation/vsomeipUserGuide.html
```

### Utiliser le hello-world

Un hello-world a été fait qui permet de tester la base de la base du SomeIP : un programme service qui attend en boucle des clients en renvoyant *Hello* suivi du message provenant du client ; et un programme client qui envoit *World*, reçoit le message du service et affchie dans le terminal *Sending: World* suivi de *Received: Hello World*. Tout un tas d'infos sont aussi écrites dans le terminal des 2 côtés.

#### Fichier de configuration

Pour que le service et le client se trouvent, il faut préalablement modifier le fichier *helloworld-local.json* situé dans le dossier *vsomeip/examples/hello_world*. Ce fichier décrit la configuration qui sera utilisée. La première ligne, `"unicast": "134.86.56.94",` décrit l'adresse de la cible (l'adresse du service pour le client, et l'adresse du client pour le service). Dans cet hello-world, le même fichier de conf est utilisé par les deux car les deux sont lancés sur le même PC ; dans un vrai contexte, il faudra utiliser deux fichiers de conf.

Il faut changer cette adresse par une adresse valable si l'on veut que l'exemple marche. Le plus simple est de viser soi-même, soit l'addresse `127.0.0.1` correspondant au localhost quasiment partout. Donc écrivez à la place `"unicast": "127.0.0.1",`.

#### Compiler

Pour compiler, mettez vous dans le dossier *vsomeip/examples/hello_world* et créez un dossier build :

```
cd examples/hello_world
mkdir build
cd build
cmake ..
```

Ceci fait, à chaque fois que vous compilez, il vous suffira d'être dans ce dossier *build* avant de taper la commande `make`.

#### Exécuter

Pour exécuter cet exemple, ouvrez deux terminaux et placez vous dans le dossier *vsomeip/examples/hello_world/build*, qui est donc le dossier où vous venez de faire `make`.

Dans un terminal, lancez :

```
VSOMEIP_CONFIGURATION=../helloworld-local.json VSOMEIP_APPLICATION_NAME=hello_world_service ./hello_world_service
```

Dans l'autre, lancez :

```
VSOMEIP_CONFIGURATION=../helloworld-local.json VSOMEIP_APPLICATION_NAME=hello_world_client ./hello_world_client
```

#### Examples

##### Modifier le message envoyé

Dans le fichier *hello_world_client.hpp*, ligne 95 la string *World* est créée, vous pouvez mettre n'importe quoi à la place.

Dans le fichier *hello_world_service.hpp*, ligne 119 la string *Hello* est créée, vous pouvez mettre n'importe quoi à la place.

##### Lancer l'appel en boucle

Dans le fichier *hello_world_service.hpp*, ligne 133 la fonction *terminate()* est appelée, commentez-là.

Dans le fichier *hello_world_client_main.cpp*, ligne 31 après le *#endif*, changez le code pour mettre à la place :

```
    while (1) {
        if (hw_cl.init()) {
            hw_cl.start();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    /*    return 0;
    } else {
        return 1;
    }*/
}
```

Pour que la ligne `std::this_thread::sleep_for(std::chrono::milliseconds(1000));` marche, il vous faudra rajouter deux includes en haut de ce fichier :

```
#include <chrono>
#include <thread>
```

Ainsi votre client lancera une requête SomeIP vers le service toute les 1000 millisecondes. **Attention**, ce code est nul et vous empêchera de tuer le programme avec Ctrl-C. Il faudra afficher les programmes contenant *hello* avec `ps -ef | grep hello` et tuer les PIDs correspondants.

### Utiliser les autres examples

Des exemples de service-client et de notify-subscribe sont aussi disponibles. Les fichiers sources modifiables sont trouvables dans le dossier *vsomeip/examples* et s'appellent respectivement *response-sample.cpp, request-sample.cpp, notify-sample.cpp* et *subscribe-sample.cpp.*

Il vous faudra modifier la ligne *unicast* dans le fichier *vsomeip-local.json* situé dans le dossier *vsomeip/config* pour y écrire  `"unicast": "127.0.0.1",`.

Pour les compiler, il faut vous rendre dans le dossier *vsomeip/build/examples* et utiliser la fonction `make`.

Pour éxécuter le **service-client**, il faudra rester dans le dossier et lancer dans un terminal :

```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=client-sample ./request-sample
```

Et dans l'autre :

```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=service-sample ./response-sample
```

Pour éxécuter le **notify-subscribe**, il faudra rester dans le dossier et lancer dans un terminal :

```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=service-sample ./notify-sample 
```

Et dans l'autre :

```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=client-sample ./subscribe-sample
```

si vous arrivez pas à lancer les exécutables(sur Ubuntu en général) et vous avez l'erreur `1 Configuration module could not be loaded!` vous devez lancer cette commande sur le terminal : `export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH` . 
`/usr/local/lib/` c'est l'emplacement courant pour les bibliothèques qui sont installées manuellement par les utilisateurs (dans notre cas vsomeip), par opposition à celles installées via le gestionnaire de packages du système.
l'erreur s'est produite parce que l'application n'a pas pu trouver les bibliothèques partagées nécessaires au moment de l'exécution. En exportant le LD_LIBRARY_PATH avec /usr/local/lib/ inclus, on demande au système de vérifier ce répertoire pour les bibliothèques partagées requises.

#### Communication entre 2 RaspberryPI

Après avoir connecté les 2 Raspberry Pi au même réseau (wifi/ethernet),il faut modifier l'adresse IP dans le fichier de config  de chaque Raspberry Pi avec son adresse IP. Ensuite, il faut ouvrir les ports localement dans la chaîne Raspberry Pi en in/out et ajouter le route pour le multicast  par les commandes ci-dessous (si vous utilisez un autre port veuillez le/les changer):
```
sudo iptables -A OUTPUT -p udp --dport 30509 -j ACCEPT
```

```
sudo iptables -A INPUT -p udp --dport 30509 -j ACCEPT
```

```
sudo route add -nv 224.244.224.245 dev wlan0
```
pour faire marché la communication partialement :) on'a utilisé le fichier  `vsomeip-udp-client.json` coté client et `vsomeip-udp-service.json`  coté service !

```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-udp-service.json VSOMEIP_APPLICATION_NAME=service-sample ./response-sample
```
```
env VSOMEIP_CONFIGURATION=../../config/vsomeip-udp-client.json VSOMEIP_APPLICATION_NAME=client-sample ./response-sample
```

#### Liens de debug utiles
https://github.com/COVESA/vsomeip/wiki/vsomeip-in-10-minutes#subscribe
https://stackoverflow.com/questions/53105172/vsomeip-communication-between-2-devices-tcp-udp-not-working
https://github.com/COVESA/vsomeip/issues/544
https://github.com/COVESA/vsomeip/issues/735

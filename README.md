# VesperCSI

**High Performance And Extensible CSI Multitool For Intel AX200/AX210 Wi-Fi Cards**

_VesperCSI is a hard fork of FeitCSI._

# Dependencies

- For VesperCSI Executable:

```shell
# For Debian-based Linux Distributions
sudo apt install libnl-3-dev libnl-genl-3-dev libconfig-dev
```

- For VesperCSI Viewer
```shell
# For Debian-based Linux Distributions
sudo apt install libgtk-4-dev libglib2.0-dev
```

# Installation

```shell
cd vespercsi
./configure --targets=vespercsi
make
sudo make install
```

# Documentation

To access the documentation, you first need to compile it with doxygen:

```shell
./configure --doxygen
```

Documentation can be accessed via HTML

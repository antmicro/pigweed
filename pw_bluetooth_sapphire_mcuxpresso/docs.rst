.. _module-pw_bluetooth_sapphire_mcuxpresso:

================================
pw_bluetooth_sapphire_mcuxpresso
================================
.. pigweed-module::
   :name: pw_bluetooth_sapphire_mcuxpresso

The ``pw_bluetooth_sapphire_mcuxpresso`` module provides a bluetooth controller
implementation that can be used together with :ref:`module-pw_bluetooth_sapphire` to implement
Bluetooth Sapphire stack.

Setup
=====
1. Use of this module requires setting up the MCUXpresso SDK for use with Pigweed. See :ref:`module-pw_build_mcuxpresso` for more details.

2. Include the bluetooth components in this SDK definition.

3. Use ``pw::bluetooth::Mimxrt595Controller`` as the controller implementation for the Bluetooth stack.

Example Hardware requirements
=============================

* Micro USB cable
* evkmimxrt595 board
* USB to serial converter
* Embedded Artists 1XK M.2 Module (EAR00385)

You can also use one of the following modules after adapting `app_bluetooth_config.h` in `mimxrt595_evk_freertos` target:

* AzureWave AW-CM358MA.M2
* AzureWave AW-CM510MA.M2
* Embedded Artists 1ZM M.2 Module (EAR00364)
* Embedded Artists 2EL M.2 Module (Rev-A1) - direct M2 connection.

Board settings
==============

Power settings:

* connect J39 with external power

Jumper settings:

* JP4 1-2
* JP7 1-2
* JP8 1-2
* JP27 1-2
* JP28 1-2
* JP29 1-2

Debug console UART:

* board UART RX (pin 1 on J27) - connect to TX pin on converter
* board UART TX (pin 2 on J27) - connect to RX pin on converter
* board GND (pin 7 on J29) - connect to GND pin on converter

UART settings:

* 115200 baud rate
* 8 data bits
* No parity
* One stop bit
* No flow control

Usage
=====

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_bluetooth_sapphire_mcuxpresso-controller-example]
   :end-before: [pw_bluetooth_sapphire_mcuxpresso-controller-example]

APIs
====

-------------------
Mimxrt595Controller
-------------------
.. doxygenclass:: pw::bluetooth::Mimxrt595Controller
   :members:

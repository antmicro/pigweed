.. _module-pw_fastboot_usb:

---------------
pw_fastboot_usb
---------------

A pluggable USB transport adapter module for :ref:`module-pw_fastboot`.

========
Overview
========

``pw_fastboot_usb`` serves as an adapter module, allowing you to use any USB
stack with :ref:`module-pw_fastboot`.

============
Requirements
============

See the `Basic Requirements (USB)
<https://android.googlesource.com/platform/system/core/+/refs/heads/main/fastboot/README.md#basic-requirements>`_
section of the fastboot protocol specification.

==========================
Integrating your USB stack
==========================

~~~~~~~~~~~~~~~~~~~~~~~~~~
High-level USB integration
~~~~~~~~~~~~~~~~~~~~~~~~~~

To be visible as a fastboot device, your USB stack must present the correct
descriptor configuration:

1. The device configuration descriptor must contain a fastboot USB interface
   descriptor. The interface must define two endpoints: in and out.

2. The Max packet size specified on the endpoints must be:

   - 64 bytes for full-speed

   - 512 bytes for high-speed

   - 1024 bytes for super-speed

3. USB Class value found on the fastboot interface descriptor must be
   ``Vendor Specific`` / ``0xff``.

4. USB Subclass value found on the fastboot interface descriptor must be ``0x42``.

5. USB Protocol value found on the fastboot interface descriptor must be ``0x03``.

6. *(optional)* The device descriptor may specify the serial number string descriptor, which
   will cause the ``fastboot`` tool to print the serial number in the device listing.
   For example:

   .. code-block::

      $ sudo fastboot devices
      0011223344    fastboot

~~~~~~~~~~~~~~~~~~~~~
USB stack integration
~~~~~~~~~~~~~~~~~~~~~

``pw_fastboot_usb`` expects an implementation of the
``pw::fastboot::UsbPacketInterface`` interface that interacts with the
underlying USB stack. A valid implementation needs to:

1. Call the ``OnPacketReceived`` method with data received from the host over
   the bulk out endpoint.

2. Provide ``QueuePacket``, which should queue the specified packet to be sent
   back to the host over the bulk in endpoint.

3. Call the ``OnPacketSent`` method once the packet previously queued with
   ``QueuePacket`` was sent to the host.

=====================
Using pw_fastboot_usb
=====================

To use ``pw_fastboot_usb``, simply use any implementation of
``pw::fastboot::UsbPacketInterface`` to instantiate a
``pw::fastboot::UsbTransport`` object:

.. code-block:: cpp

   auto transport = std::make_unique<pw::fastboot::UsbTransport>(
       std::make_unique<Mimxrt595UsbPacketInterface>());

You can directly pass an ``pw::fastboot::UsbTransport`` object as the transport
during construction of ``pw::fastboot::Device``.

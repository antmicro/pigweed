.. _module-pw_flash_mcuxpresso:

-------------------
pw_flash_mcuxpresso
-------------------
``pw_flash_mcuxpresso`` implements the :ref:`module-pw_flash` interface using
the NXP MCUXpresso SDK.


Setup
=====
Use of this module requires setting up the MCUXpresso SDK for use with Pigweed.
Follow the steps in :ref:`module-pw_build_mcuxpresso` to configure the SDK based
on your preferred build system. Additionally, this module requires that the
``mflash`` component is included in the SDK. This can be done by adding it to
the included component list on the SDK rule.

For example, to enable ``mflash`` for use with the MIMXRT595S on a Pigweed
project utilizing Bazel, configure the ``mcuxpresso_sdk`` rule as follows:

.. code-block:: python

   mcuxpresso_sdk(
     # ...
     includes = [
         "component.mflash.rt595.MIMXRT595S",
     ],
     # ...
   )


Examples
========
Use the ``pw::flash::McuxpressoFlash`` class to read, erase and program the
onboard SPI flash.

Example code to erase and reprogram the first 4096 bytes of SPI flash with
``0x42`` bytes:

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash_mcuxpresso-example]
   :end-before: [pw_flash_mcuxpresso-example]


API Reference
=============

.. doxygenclass:: pw::flash::McuxpressoFlash
  :members:

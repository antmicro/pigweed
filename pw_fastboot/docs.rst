.. _module-pw_fastboot:

-----------
pw_fastboot
-----------

The ``pw_fastboot`` module contains the device-side implementation of the
Android `fastboot
<https://android.googlesource.com/platform/system/core/+/refs/heads/main/fastboot/README.md>`_
protocol used for communicating with bootloaders over USB or Ethernet.

===============
Getting started
===============

Use of this module requires providing a custom transport implementation,
depending on the desired fastboot transport method (USB / UDP / TCP).

For a reference implementation of a transport, see
:ref:`module-pw_fastboot_usb`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Registering command handlers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To register handlers for specific fastboot commands, define a custom class
implementing the ``pw::fastboot::DeviceHAL`` interface:

.. code-block:: cpp

   class MyDeviceHal : public pw::fastboot::DeviceHAL {
   public:
   constexpr MyDeviceHal() = default;

   pw::fastboot::CommandResult Flash(pw::fastboot::Device* device,
                                     std::string name) override {
       return pw::fastboot::CommandResult::Failed("Command unimplemented!");
   }

   pw::fastboot::CommandResult Reboot(pw::fastboot::Device*,
                                      pw::fastboot::RebootType) override {
       return pw::fastboot::CommandResult::Failed("Command unimplemented!");
   }

   pw::fastboot::CommandResult ShutDown(pw::fastboot::Device*) override {
       return pw::fastboot::CommandResult::Failed("Command unimplemented!");
   }

   pw::fastboot::CommandResult OemCommand(pw::fastboot::Device*,
                                          std::string) override {
       return pw::fastboot::CommandResult::Failed("Command unimplemented!");
   }

   bool IsDeviceLocked(pw::fastboot::Device*) override { return false; }

   private:
   };

Then, instantiate a ``std::unique_ptr`` of this class which will be used for
registering the commands later:

.. code-block:: cpp

   auto hal = std::make_unique<MyDeviceHal>();

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Customizing fastboot variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The fastboot protocol allows exposing certain bootloader variables, which can be
done in ``pw_fastboot`` by using the ``pw::fastboot::VariableProvider`` object
provided to the ``pw::fastboot::Device`` class.

For example, to register a bootloader variable ``foobar`` that will return the
string ``Hello, world!`` when queried:

.. code-block:: cpp

   auto variables = std::make_unique<pw::fastboot::VariableProvider>();
   variables->RegisterVariable("foobar",
                               [](auto, auto, std::string* message) -> bool {
                                   *message = "Hello, world!";
                                   return true;
                               });

Registered variables are available using the ``getvar`` command from a host PC
to which the fastboot device is connected to:

.. code-block:: text

   $ sudo fastboot getvar foobar
   foobar: Hello, world!
   Finished. Total time: 0.000s

``pw_fastboot`` supports the ``all`` meta-variable, which can be used to query
all registered variables at once. If a variable should not be present in the
output of ``getvar all`` while still allowing regular fetching via ``getvar``,
you can instead register a special variable:

.. code-block:: cpp

   auto variables = std::make_unique<pw::fastboot::VariableProvider>();
   variables->RegisterSpecialVariable(
       "barbaz", [](pw::fastboot::Device* device) -> bool {
           device->WriteInfo("Hidden variable, invisible from getvar all");
           return true;
       });

Special variables can also return multi-line strings by calling ``WriteInfo``
multiple times.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Starting the fastboot protocol loop
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To start receiving fastboot commands from the configured transport, create a
``pw::fastboot::Device`` object:

.. code-block:: cpp

   pw::fastboot::Device device{std::make_unique<MyFastbootTransport>(),
                               std::move(variables),
                               std::move(hal)};
   device.ExecuteCommands();

.. admonition:: Note

    The call to ``pw::fastboot::Device::ExecuteCommands`` will block
    indefinitely. When using an RTOS, you can explicitly terminate command
    processing by calling ``pw::fastboot::Device::CloseDevice``.

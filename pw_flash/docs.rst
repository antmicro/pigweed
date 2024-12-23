.. _module-pw_flash:

--------
pw_flash
--------
``pw_flash`` provides a cross-platform interface for reading and writing flash
storage. It can be used to abstract over the underlying storage device, enabling
cross-platform storage operations.

An example use of ``pw_flash`` could involve writing some data to on-board flash
of a device for persistent storage.


Overview
========
``pw_flash`` treats the flash memory as a collection of erasable pages. Pages
are organized into regions, which can be queried on the ``pw::flash::Flash``
object.

The region layout is represented using a run-length encoded array. Each entry
specifies the number of pages in a region and the size of each page.

Most flash devices have uniform page sizes and will only contain a single region
covering the entire available storage space. For example, a 64 MiB flash device
with a page size of 4096 would contain the following regions:

.. code-block::

   Region 1: [16384 pages of 4096 bytes each]

And can be visualized as:

.. code-block::

   +------------------+------------------+------------------+
   | 4096 bytes       | 4096 bytes       | .. (16384 total) |
   +------------------+------------------+------------------+
   0                                                0x3FFFFFF

In the case of non-uniform flash devices, the layout will contain multiple
regions, each with a different page size. For example:

.. code-block::

   Region 1: [4 pages of 512 bytes each]
   Region 2: [2 pages of 1024 bytes each]
   Region 3: [1 page of 2048 bytes]

Which can be visualized as:

.. code-block::

   +-------+-------+-------+-------+
   | 512 b | 512 b | 512 b | 512 b |
   +-------+-------+-------+-------+
   0                            2047
   +-------+-------+-------+-------+
   | 1024 bytes    | 1024 bytes    |
   +---------------+---------------+
   2048                         4095
   +---------------+---------------+
   | 2048 bytes                    |
   +-------------------------------+
   4096                         6143


Erasing flash
-------------

An erase operation can be performed on one or more pages at a time. This can be
specified using the range passed in the call to ``Erase()``, which must
completely cover all pages to be erased.

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-erase-example]
   :end-before: [pw_flash-erase-example]

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-erase-many-example]
   :end-before: [pw_flash-erase-many-example]


Writing flash
-------------

A write operation can be performed on one or more pages at a time. The pages
must have previously been erased for this operation to succeed. All ``pw_flash``
drivers must support writing one or more full pages as defined in the layout,
but most often it is possible to write to a page with a smaller granularity. The
actual minimum writable block size can be inspected by using
``GetFlashParameters()``.

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-write-example]
   :end-before: [pw_flash-write-example]

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-write-partial-example]
   :end-before: [pw_flash-write-partial-example]


Reading flash
-------------

A read operation can be performed on one or more pages at a time. All
``pw_flash`` drivers must support reading one or more full pages as defined in
the layout.

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-read-example]
   :end-before: [pw_flash-read-example]

.. literalinclude:: example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_flash-read-many-example]
   :end-before: [pw_flash-read-many-example]


API Reference
=============

.. doxygenclass:: pw::flash::Flash
  :members:

.. doxygenstruct:: pw::flash::Range
  :members:

.. doxygenstruct:: pw::flash::FlashParams
  :members:

.. doxygenstruct:: pw::flash::PageLayout
  :members:

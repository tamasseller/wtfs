Wandering Tree File System
==========================

Very small resource footprint, robust filesystem for _raw NAND flash_ devices. 
It is designed to use so little RAM that it can be deployed in on contemporary middle to high-end 32-bit MCU based systems.
Efficient and robust due to its state-of-the-art wandering B+ tree based storage structure.

Summary of goals
----------------

 - Tiny RAM foorptint
 - Designed-in robustness
 - High throughput
 - Usability in both single and multi-threaded (RTOS) environments.
 - Independence of OS, compiler and standard libraries.
 - Ability to stay mounted with the use of a little non-volatile memory (ie backup ram or built-in flash).
 
 It was also a secondary intention to make use of modern compiler technology to enhance the overall system 
 performance enabled by agressive inlineing and.

How to use it
-------------

To acheive high performance and wide usability at the same time with the same code base it is necessary to
leave many things configurable for the integrator. All of the configuration data are injected through a single
template parameter, which 

![Alt text](https://g.gravizo.com/g?
  digraph G {
    wtfs[shape=record label="<l>wtfs\\nlogic|<b>buffers|<m>manager"];
    app[label = "Application"];
    drv[label = "Driver"];
    edge[penwidth=3]
    app -> wtfs:b [label = "Direct\\naccess"];
    wtfs:b -> drv[label = "DMA"];
    edge[penwidth="" style=dashed]
    app -> wtfs:l [label = "Meta\\nops"];
    wtfs:m -> drv[label = "Commands"];
  }
)

How it works
------------

The structure of the stored data is based on a wandering B+ tree. 
All update operations have to write a new version of the root, which makes
This means that the whole structure has a single "atomically" writable entry point. 

- High throughput, achieved by employing (almost) _zero-copy_ interfaces.

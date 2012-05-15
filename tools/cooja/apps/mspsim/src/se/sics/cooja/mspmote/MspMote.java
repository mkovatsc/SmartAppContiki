/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: MspMote.java,v 1.48 2010/10/04 12:54:01 joxe Exp $
 */

package se.sics.cooja.mspmote;

import java.awt.event.ActionListener;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Observable;

import org.apache.log4j.Logger;
import org.jdom.Element;

import se.sics.cooja.ContikiError;
import se.sics.cooja.GUI;
import se.sics.cooja.Mote;
import se.sics.cooja.MoteInterface;
import se.sics.cooja.MoteInterfaceHandler;
import se.sics.cooja.MoteMemory;
import se.sics.cooja.MoteType;
import se.sics.cooja.Simulation;
import se.sics.cooja.Watchpoint;
import se.sics.cooja.WatchpointMote;
import se.sics.cooja.interfaces.IPAddress;
import se.sics.cooja.motes.AbstractEmulatedMote;
import se.sics.cooja.mspmote.interfaces.MspSerial;
import se.sics.cooja.mspmote.plugins.CodeVisualizerSkin;
import se.sics.cooja.mspmote.plugins.MspBreakpointContainer;
import se.sics.cooja.plugins.BufferListener.BufferAccess;
import se.sics.cooja.plugins.Visualizer;
import se.sics.mspsim.cli.CommandContext;
import se.sics.mspsim.cli.CommandHandler;
import se.sics.mspsim.cli.LineListener;
import se.sics.mspsim.cli.LineOutputStream;
import se.sics.mspsim.core.CPUMonitor;
import se.sics.mspsim.core.EmulationException;
import se.sics.mspsim.core.MSP430;
import se.sics.mspsim.core.MSP430Constants;
import se.sics.mspsim.platform.GenericNode;
import se.sics.mspsim.ui.JFrameWindowManager;
import se.sics.mspsim.util.ComponentRegistry;
import se.sics.mspsim.util.ConfigManager;
import se.sics.mspsim.util.DebugInfo;
import se.sics.mspsim.util.ELF;
import se.sics.mspsim.util.MapEntry;
import se.sics.mspsim.util.MapTable;
import se.sics.mspsim.util.SimpleProfiler;

/**
 * @author Fredrik Osterlind
 */
public abstract class MspMote extends AbstractEmulatedMote implements Mote, WatchpointMote {
  private static Logger logger = Logger.getLogger(MspMote.class);

  private final static int EXECUTE_DURATION_US = 1; /* We always execute in 1 us steps */

  {
    Visualizer.registerVisualizerSkin(CodeVisualizerSkin.class);
  }

  private CommandHandler commandHandler;
  private MSP430 myCpu = null;
  private MspMoteType myMoteType = null;
  private MspMoteMemory myMemory = null;
  private MoteInterfaceHandler myMoteInterfaceHandler = null;
  public ComponentRegistry registry = null;
  
  /* Stack monitoring variables */
  private boolean stopNextInstruction = false;
  private boolean monitorStackUsage = false;
  private int stackPointerLow = Integer.MAX_VALUE;
  private int heapStartAddress;
  private StackOverflowObservable stackOverflowObservable = new StackOverflowObservable();

  private MspBreakpointContainer breakpointsContainer;

  public MspMote() {
    myMoteType = null;
    myCpu = null;
    myMemory = null;
    myMoteInterfaceHandler = null;

    /* Scheduled from setConfigXML */
  }

  public MspMote(MspMoteType moteType, Simulation simulation) {
    this.simulation = simulation;
    myMoteType = moteType;

    /* Schedule us immediately */
    requestImmediateWakeup();
  }
  
  protected void initMote() {
    if (myMoteType != null) {
      initEmulator(myMoteType.getContikiFirmwareFile());
      myMoteInterfaceHandler = createMoteInterfaceHandler();

      /* TODO Setup COOJA-specific window manager */
      registry.registerComponent("windowManager", new JFrameWindowManager());

      /* Create watchpoint container */
      try {
        breakpointsContainer = new MspBreakpointContainer(this, ((MspMoteType)getType()).getFirmwareDebugInfo());
      } catch (IOException e) {
        throw (RuntimeException) new RuntimeException("Error: " + e.getMessage()).initCause(e);
      }
    }
  }

  /**
   * Abort execution immediately.
   * May for example be called by a breakpoint handler.
   */
  public void stopNextInstruction() {
    stopNextInstruction = true;
  }

  protected MoteInterfaceHandler createMoteInterfaceHandler() {
    return new MoteInterfaceHandler(this, getType().getMoteInterfaceClasses());
  }

  /**
   * @return MSP430 CPU
   */
  public MSP430 getCPU() {
    return myCpu;
  }

  public void setCPU(MSP430 cpu) {
    myCpu = cpu;
  }

  public MoteMemory getMemory() {
    return myMemory;
  }

  public void setMemory(MoteMemory memory) {
    myMemory = (MspMoteMemory) memory;
  }

  /* Stack monitoring variables */
  public class StackOverflowObservable extends Observable {
    public void signalStackOverflow() {
      setChanged();
      notifyObservers();
    }
  }

  /**
   * Enable/disable stack monitoring
   *
   * @param monitoring Monitoring enabled
   */
  public void monitorStack(boolean monitoring) {
    this.monitorStackUsage = monitoring;
    resetLowestStackPointer();
  }

  /**
   * @return Lowest SP since stack monitoring was enabled
   */
  public int getLowestStackPointer() {
    return stackPointerLow;
  }

  /**
   * Resets lowest stack pointer variable
   */
  public void resetLowestStackPointer() {
    stackPointerLow = Integer.MAX_VALUE;
  }

  /**
   * @return Stack overflow observable
   */
  public StackOverflowObservable getStackOverflowObservable() {
    return stackOverflowObservable;
  }

  /**
   * Prepares CPU, memory and ELF module.
   *
   * @param fileELF ELF file
   * @param cpu MSP430 cpu
   * @throws IOException Preparing mote failed
   */
  protected void prepareMote(File fileELF, GenericNode node) throws IOException {
    this.commandHandler = new CommandHandler(System.out, System.err);
    node.setCommandHandler(commandHandler);

    ConfigManager config = new ConfigManager();
    node.setup(config);

    this.myCpu = node.getCPU();
    this.myCpu.setMonitorExec(true);
    this.myCpu.setTrace(0); /* TODO Enable */

    int[] memory = myCpu.getMemory();
    logger.info("Loading firmware from: " + fileELF.getAbsolutePath());
    GUI.setProgressMessage("Loading " + fileELF.getName());
    node.loadFirmware(((MspMoteType)getType()).getELF(), memory);

    /* Throw exceptions at bad memory access */
    /*myCpu.setThrowIfWarning(true);*/

    /* Create mote address memory */
    MapTable map = ((MspMoteType)getType()).getELF().getMap();
    MapEntry[] allEntries = map.getAllEntries();
    myMemory = new MspMoteMemory(this, allEntries, myCpu);

    heapStartAddress = map.heapStartAddress;
    myCpu.reset();
  }

  public CommandHandler getCLICommandHandler() {
    return commandHandler;
  }

  /* called when moteID is updated */
  public void idUpdated(int newID) {
  }

  public MoteType getType() {
    return myMoteType;
  }

  public void setType(MoteType type) {
    myMoteType = (MspMoteType) type;
  }

  public MoteInterfaceHandler getInterfaces() {
    return myMoteInterfaceHandler;
  }

  public void setInterfaces(MoteInterfaceHandler moteInterfaceHandler) {
    myMoteInterfaceHandler = moteInterfaceHandler;
  }

  /**
   * Initializes emulator by creating CPU, memory and node object.
   *
   * @param ELFFile ELF file
   * @return True if successful
   */
  protected abstract boolean initEmulator(File ELFFile);

  private boolean booted = false;

  public void simTimeChanged(long diff) {
    /* Compensates for simulation time changes (without simulation execution) */
    lastExecute -= diff;
    nextExecute -= diff;
    scheduleNextWakeup(nextExecute);
  }

  private long lastExecute = -1; /* Last time mote executed */
  private long nextExecute;
  public void execute(long time) {
    execute(time, EXECUTE_DURATION_US);
  }
  public void execute(long t, int duration) {
    /* Wait until mote boots */
    if (!booted && myMoteInterfaceHandler.getClock().getTime() < 0) {
      scheduleNextWakeup(t - myMoteInterfaceHandler.getClock().getTime());
      return;
    }
    booted = true;

    if (stopNextInstruction) {
      stopNextInstruction = false;
      /*sendCLICommandAndPrint("trace 1000");*/ /* TODO Enable */
      scheduleNextWakeup(t);
      throw new RuntimeException("MSPSim requested simulation stop");
    } 

    if (lastExecute < 0) {
      /* Always execute one microsecond the first time */
      lastExecute = t;
    }
    if (t < lastExecute) {
      throw new RuntimeException("Bad event ordering: " + lastExecute + " < " + t);
    }
    
    /* Execute MSPSim-based mote */
    /* TODO Try-catch overhead */
    try {
      nextExecute = 
        t + duration + 
        myCpu.stepMicros(t - lastExecute, duration);
      lastExecute = t;
    } catch (EmulationException e) {
      String trace = e.getMessage() + "\n\n" + getStackTrace();
      throw (ContikiError)
      new ContikiError(trace).initCause(e);
    }

    /* Schedule wakeup */
    if (nextExecute < t) {
      throw new RuntimeException(t + ": MSPSim requested early wakeup: " + nextExecute);
    }
    /*logger.debug(t + ": Schedule next wakeup at " + nextExecute);*/
    scheduleNextWakeup(nextExecute);
    
    
    /* XXX TODO Reimplement stack monitoring using MSPSim internals */
    /*if (monitorStackUsage) {
      int newStack = cpu.reg[MSP430.SP];
      if (newStack < stackPointerLow && newStack > 0) {
        stackPointerLow = cpu.reg[MSP430.SP];

        // Check if stack is writing in memory
        if (stackPointerLow < heapStartAddress) {
          stackOverflowObservable.signalStackOverflow();
          stopNextInstruction = true;
          getSimulation().stopSimulation();
        }
      }
    }*/
  }
  
  public String getStackTrace() {
    return executeCLICommand("stacktrace");
  }

  public int executeCLICommand(String cmd, CommandContext context) {
    return commandHandler.executeCommand(cmd, context);
  }
  
  public String executeCLICommand(String cmd) {
    final StringBuilder sb = new StringBuilder();
    LineListener ll = new LineListener() {
      public void lineRead(String line) {
        sb.append(line).append("\n");
      }
    };
    PrintStream po = new PrintStream(new LineOutputStream(ll));
    CommandContext c = new CommandContext(commandHandler, null, "", new String[0], 1, null);
    c.out = po;
    c.err = po;
    
    if (0 != executeCLICommand(cmd, c)) {
      sb.append("\nWarning: command failed");
    }
    
    return sb.toString();
  }
  
  public int getCPUFrequency() {
    return myCpu.getDCOFrequency();
  }

  public int getID() {
    return getInterfaces().getMoteID().getMoteID();
  }
  
  public boolean setConfigXML(Simulation simulation, Collection<Element> configXML, boolean visAvailable) {
    setSimulation(simulation);
    if (myMoteInterfaceHandler == null) {
      myMoteInterfaceHandler = createMoteInterfaceHandler();
    }

    /* Create watchpoint container */
    try {
      breakpointsContainer = new MspBreakpointContainer(this, ((MspMoteType)getType()).getFirmwareDebugInfo());
    } catch (IOException e) {
      throw (RuntimeException) new RuntimeException("Error: " + e.getMessage()).initCause(e);
    }

    for (Element element: configXML) {
      String name = element.getName();

      if (name.equals("motetype_identifier")) {
        /* Ignored: handled by simulation */
      } else if ("breakpoints".equals(element.getName())) {
        breakpointsContainer.setConfigXML(element.getChildren(), visAvailable);
      } else if (name.equals("interface_config")) {
        String intfClass = element.getText().trim();
        if (intfClass.equals("se.sics.cooja.mspmote.interfaces.MspIPAddress")) {
          intfClass = IPAddress.class.getName();
        }
        if (intfClass.equals("se.sics.cooja.mspmote.interfaces.ESBLog")) {
          intfClass = MspSerial.class.getName();
        }
        if (intfClass.equals("se.sics.cooja.mspmote.interfaces.SkySerial")) {
          intfClass = MspSerial.class.getName();
        }
        Class<? extends MoteInterface> moteInterfaceClass = simulation.getGUI().tryLoadClass(
              this, MoteInterface.class, intfClass);

        if (moteInterfaceClass == null) {
          logger.fatal("Could not load mote interface class: " + intfClass);
          return false;
        }

        MoteInterface moteInterface = getInterfaces().getInterfaceOfType(moteInterfaceClass);
        moteInterface.setConfigXML(element.getChildren(), visAvailable);
      }
    }

    /* Schedule us immediately */
    requestImmediateWakeup();
    return true;
  }

  public Collection<Element> getConfigXML() {
    ArrayList<Element> config = new ArrayList<Element>();
    Element element;

    /* Breakpoints */
    element = new Element("breakpoints");
    element.addContent(breakpointsContainer.getConfigXML());
    config.add(element);

    // Mote interfaces
    for (MoteInterface moteInterface: getInterfaces().getInterfaces()) {
      element = new Element("interface_config");
      element.setText(moteInterface.getClass().getName());

      Collection<Element> interfaceXML = moteInterface.getConfigXML();
      if (interfaceXML != null) {
        element.addContent(interfaceXML);
        config.add(element);
      }
    }

    return config;
  }


  /* Watchpoints: Forward to breakpoint container */
  public void addWatchpointListener(ActionListener listener) {
    breakpointsContainer.addWatchpointListener(listener);
  }

  public Watchpoint getLastWatchpoint() {
    return breakpointsContainer.getLastWatchpoint();
  }

  public Mote getMote() {
    return breakpointsContainer.getMote();
  }

  public ActionListener[] getWatchpointListeners() {
    return breakpointsContainer.getWatchpointListeners();
  }

  public void removeWatchpointListener(ActionListener listener) {
    breakpointsContainer.removeWatchpointListener(listener);
  }

  public MspBreakpointContainer getBreakpointsContainer() {
    return breakpointsContainer;
  }

  public String getExecutionDetails() {
    return executeCLICommand("stacktrace");
  }

  public String getPCString() {
    int pc = myCpu.getPC();
    ELF elf = (ELF)myCpu.getRegistry().getComponent(ELF.class); 
    DebugInfo di = elf.getDebugInfo(pc);
    
    /* Following code examples from MSPsim, DebugCommands.java */
    if (di == null) {
      di = elf.getDebugInfo(pc + 1);
    }
    if (di == null) {
      /* Return PC value */
      SimpleProfiler sp = (SimpleProfiler)myCpu.getProfiler();
      try {
        MapEntry mapEntry = sp.getCallMapEntry(0);
        if (mapEntry != null) {
          String file = mapEntry.getFile();
          if (file != null) {
            if (file.indexOf('/') >= 0) {
              file = file.substring(file.lastIndexOf('/')+1);
            }
          }
          String name = mapEntry.getName();
          return file + ":?:" + name; 
        }
        return String.format("*%02x", myCpu.reg[MSP430Constants.PC]);
      } catch (Exception e) {
        return null;
      }
    }

    int lineNo = di.getLine();
    String file = di.getFile();
    file = file==null?"?":file;
    if (file.contains("/")) {
      /* strip path */
      file = file.substring(file.lastIndexOf('/')+1, file.length());
    }
    
    String function = di.getFunction();
    function = function==null?"":function;
    if (function.contains(":")) {
      /* strip arguments */
      function = function.substring(0, function.lastIndexOf(':'));
    }
    if (function.equals("* not available")) {
      function = "?";
    }
    return file + ":" + lineNo + ":" + function;
    
    /*return executeCLICommand("line " + myCpu.getPC());*/
  }
}

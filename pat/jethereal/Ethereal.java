package jethereal;

import java.awt.Component;
import java.awt.Container;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.util.List;

import javax.swing.JApplet;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JTable;
import javax.swing.JTree;
import javax.swing.KeyStroke;
import javax.swing.ListSelectionModel;
import javax.swing.WindowConstants;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;
import javax.swing.table.TableModel;
import javax.swing.tree.TreeModel;
import javax.swing.tree.TreeSelectionModel;

import jethereal.parser.Registry;
import jpcap.JpcapCaptor;
import jpcap.PacketReceiver;
import jpcap.packet.Packet;

public class Ethereal extends JApplet implements ActionListener, WindowListener {

	private EtherealTableModel model;

	private JFrame frame;

	private String errorString = "It appears you don't have jpcap or libpcap (winpcap) installed. Please see the instructions on the side of the page on how to get these libraries installed.\n\n It takes about 30 seconds to install them and they are easy to uninstall, so I highly recomend that you go and download them to try out the live packet capture features";

	public Ethereal() {
		
	}

	public void init() {
		Container contentPane = this.getContentPane();
		contentPane
				.add(new JLabel(
						"A window has been opened. Check your taskbar to see it. Refresh this page to reopen it."));
		contentPane.repaint();

		Registry.init();
		String header = "JEthereal";
		makeFrame(header);
	}

	/*
	 * This method picks good column sizes.
	 * If all column heads are wider than the column's cells'
	 * contents, then you can just use column.sizeWidthToFit().
	 */
	public static void initColumnSizes(JTable table, Object[] longValues) {
		TableModel model = table.getModel();
		TableColumn column = null;
		Component comp = null;
		int headerWidth = 0;
		int cellWidth = 0;
		TableCellRenderer headerRenderer = table.getTableHeader()
				.getDefaultRenderer();

		for (int i = 0; i < longValues.length; i++) {
			column = table.getColumnModel().getColumn(i);

			comp = headerRenderer.getTableCellRendererComponent(table, column
					.getHeaderValue(), false, false, 0, 0);
			headerWidth = comp.getPreferredSize().width;

			comp = table.getDefaultRenderer(model.getColumnClass(i))
					.getTableCellRendererComponent(table, longValues[i], false,
							false, 0, i);
			cellWidth = comp.getPreferredSize().width;

			//XXX: Before Swing 1.1 Beta 2, use setMinWidth instead.
			column.setPreferredWidth(Math.max(headerWidth, cellWidth));
		}
	}

	private boolean hasJPcap(boolean print) {
		try {
			System.out.println("Looking for libjpcap in: "
					+ System.getProperty("java.library.path"));
			Class c = Class.forName("jpcap.JpcapCaptor");
		} catch (ClassNotFoundException e) {
			if (print)
				JOptionPane.showMessageDialog(this, errorString);
			e.printStackTrace();
			return false;
		} catch (UnsatisfiedLinkError e) {
			if (print)
				JOptionPane.showMessageDialog(this, errorString);
			e.printStackTrace();
			
			// Try to download the DLL for them
			
			try {
				URL url = this.getDocumentBase();
				
				url = new URL(url + "/Jpcap.dll");
				
				File temp = File.createTempFile("Jpcap", ".dll");

				// Delete temp file when program exits.
				temp.deleteOnExit();
				OutputStream out = new DataOutputStream(new FileOutputStream(temp));
				
				InputStream in = new DataInputStream(url.openStream());
				
				byte[] buffer= new byte[1024];
				
				int len;

				while ((len = in.read(buffer)) != -1)
				    out.write(buffer, 0, len);
				
				out.close();
				
				try {
					System.load(temp.getAbsolutePath());
					System.out.println("Downloaded and saved dll");
					
				} catch (UnsatisfiedLinkError e1) {
					e1.printStackTrace();
					return false;
				}

			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
				return false;
			}
	        
		}
		System.out.println("Found it");
		return true;

	}

	private void makeFrame(String name) {

		JTable table = new JTable();
		JScrollPane scroll1 = new JScrollPane(table);

		JTree tree = new JTree((TreeModel) null);

		JScrollPane scroll2 = new JScrollPane(tree);
		HexTable hexTable = new HexTable();
		JScrollPane scroll3 = new JScrollPane(hexTable);

		// Without sorter
		//table.setModel(model);

		model = new EtherealTableModel(tree, hexTable);

		// With sorter
		TableSorter sorter = new TableSorter(model);
		table.setModel(sorter);
		sorter.setTableHeader(table.getTableHeader());

		model.setSorter(sorter);

		table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
		table.getSelectionModel().addListSelectionListener(model);

		tree.addTreeSelectionListener(model);
		tree.getSelectionModel().setSelectionMode(
				TreeSelectionModel.SINGLE_TREE_SELECTION);

		hexTable.setRowSelectionAllowed(true);
		hexTable.setColumnSelectionAllowed(true);
		hexTable.setCellSelectionEnabled(true);
		hexTable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
		hexTable.getSelectionModel().addListSelectionListener(model);
		hexTable.getColumnModel().getSelectionModel().addListSelectionListener(
				model);

		initColumnSizes(table, EtherealTableModel.longValues);
		//table.setAutoResizeMode(JTable.AUTO_RESIZE_OFF);
		scroll1.setPreferredSize(table.getPreferredSize());

		frame = new JFrame(name);
		JSplitPane top = new JSplitPane(JSplitPane.VERTICAL_SPLIT, scroll1,
				scroll2);
		JSplitPane bottem = new JSplitPane(JSplitPane.VERTICAL_SPLIT, top,
				scroll3);
		top.setDividerLocation(300);
		bottem.setDividerLocation(600);

		JMenuBar menuBar = new JMenuBar();
		JMenu file = new JMenu("File");
		file.setMnemonic('F');
		JMenuItem openMenu = new JMenuItem("Open");
		openMenu.setMnemonic('O');
		openMenu.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_O,
				ActionEvent.CTRL_MASK));
		openMenu.addActionListener(this);
		file.add(openMenu);
		JMenuItem saveMenu = new JMenuItem("Save");
		saveMenu.setMnemonic('S');
		saveMenu.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_S,
				ActionEvent.CTRL_MASK));
		saveMenu.addActionListener(this);
		file.add(saveMenu);
		
		JMenuItem clearMenu = new JMenuItem("Clear");
		clearMenu.setMnemonic('C');
		clearMenu.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_C,
				ActionEvent.CTRL_MASK));
		clearMenu.addActionListener(this);
		file.add(clearMenu);
		
		JMenuItem exitMenu = new JMenuItem("Exit");
		exitMenu.setMnemonic('x');
		exitMenu.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_Q,
				ActionEvent.CTRL_MASK));
		exitMenu.addActionListener(this);
		file.add(exitMenu);

		JMenu capture = new JMenu("Capture");
		capture.setMnemonic('C');
		JMenuItem startCap = new JMenuItem("Start Capturing");
		startCap.setMnemonic('S');
		startCap.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_A,
				ActionEvent.CTRL_MASK));
		startCap.addActionListener(this);
		capture.add(startCap);
		JMenuItem endCap = new JMenuItem("Stop Capturing");
		endCap.setMnemonic('t');
		endCap.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_Z,
				ActionEvent.CTRL_MASK));
		endCap.addActionListener(this);
		capture.add(endCap);

		JMenu help = new JMenu("Help");
		help.setMnemonic('H');
		JMenuItem why = new JMenuItem("Why are some options greyed out?");
		why.addActionListener(this);
		help.add(why);
		JMenuItem about = new JMenuItem("About");
		about.addActionListener(this);
		help.add(about);

		menuBar.add(file);
		menuBar.add(capture);
		menuBar.add(help);
		frame.setJMenuBar(menuBar);

		//frame.add(scroll1, BorderLayout.NORTH);
		//frame.add(scroll2, BorderLayout.CENTER);
		//frame.add(scroll3, BorderLayout.SOUTH);
		frame.getContentPane().add(bottem); // bug fix for mac
		frame.pack();
		frame.setVisible(true);
		frame.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);
		frame.addWindowListener(this);
		JFrame.setDefaultLookAndFeelDecorated(true);


		startCap.setEnabled(false);
		endCap.setEnabled(false);
		
		if (hasJPcap(false)) {
			startCap.setEnabled(true);
			endCap.setEnabled(true);
		}
	}

	public synchronized void addPacket(EtherealPacket packet) {
		model.addPacket(packet);
	}

	public void setName(String name) {
		super.setName(name);
		frame.setTitle(name);
	}

	public boolean isModifying() {
		return false;
	}

	public void showWindow() {
		frame.setVisible(true);
	}

	public void clear() {
		model.clear();
	}

	JFileChooser chooser = new JFileChooser();

	public void actionPerformed(ActionEvent event) {
		Object source = event.getSource();
		if (source instanceof JMenuItem) {
			JMenuItem menu = (JMenuItem) source;
			if (menu.getText().equals("Open")) {
				int returnVal = chooser.showOpenDialog(frame);
				if (returnVal == JFileChooser.APPROVE_OPTION) {
					System.out.println("You chose to open this file: "
							+ chooser.getSelectedFile().getName());
				} else if (returnVal == JFileChooser.CANCEL_OPTION)
					return;
				/*
				 try {
				 jpcap = JpcapCaptor.openFile(chooser.getSelectedFile().toString());
				 startCaptureThread();
				 } catch (IOException e1) {
				 // TODO Auto-generated catch block
				 e1.printStackTrace();
				 }
				 */

				try {
					List<EtherealPacket> packets = Parser.read(chooser
							.getSelectedFile());
					if (packets != null) {
						for (EtherealPacket i : packets) {
							addPacket(i);
						}
					} else {
						JOptionPane
								.showMessageDialog(
										this,
										"The file you selected: \""
												+ chooser.getSelectedFile()
														.getName()
												+ "\" couldn't be parsed. It is either corupted or not a pcap file. Please choose another file.");
					}
				} catch (FileNotFoundException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} catch (IOException e) {
					// TODO Auto-generated catch block
					JOptionPane.showMessageDialog(frame,
							"Can't open file: " + chooser.getSelectedFile().getPath() + "\n\n"
									+ e.toString());
					e.printStackTrace();
				}
			} else if (menu.getText().equals("Save")) {
				int ret = chooser.showSaveDialog(frame);
				if (ret == JFileChooser.APPROVE_OPTION) {
					File file = chooser.getSelectedFile();
					if (file.exists()) {
						if (JOptionPane.showConfirmDialog(frame, "Overwrite "
								+ file.getName() + "?", "Overwrite?",
								JOptionPane.YES_NO_OPTION) == JOptionPane.NO_OPTION) {
							return;
						}
					}
					
					try {
						Parser.write(model.getPackets(), file);
						/*
						JpcapWriter writer = JpcapWriter.openDumpFile(jpcap,
								file.getPath());

						ArrayList<EtherealPacket> packets = model.getPackets();
						for (EtherealPacket p : packets)
							writer.writePacket(p.getJPCapPacket());

						writer.close();
						*/
						
					} catch (java.io.IOException e) {
						e.printStackTrace();
						JOptionPane.showMessageDialog(frame,
								"Can't save file: " + file.getPath() + "\n\n"
										+ e.toString());
					}
				}
			} else if (menu.getText().equals("Save")) {
				clear();
			} else if (menu.getText().equals("Exit")) {
				stopCaptureThread();
				this.frame.dispose();
			} else if (menu.getText().equals("Start Capturing")) {

				try {
					jpcap = JDCaptureDialog.getJpcap(frame);
					//Class c = Class.forName("jpcap.JpcapCaptor");
				//} catch (ClassNotFoundException e) {
					//JOptionPane.showMessageDialog(frame, errorString);
					//return;
				} catch (UnsatisfiedLinkError e) {
					JOptionPane.showMessageDialog(frame, errorString);
					return;
				}

				if (jpcap != null)
					startCaptureThread();

			} else if (menu.getText().equals("Stop Capturing")) {
				stopCaptureThread();
			} else if (menu.getText()
					.equals("Why are some options greyed out?")) {
				JOptionPane.showMessageDialog(frame, errorString);
			} else if (menu.getText().equals("About")) {
				JOptionPane.showMessageDialog(frame,
						"Developed by Paul Tarjan at Stanford University");
			}
		}
	}

	JpcapCaptor jpcap = null;

	private Thread captureThread;

	private void startCaptureThread() {
		if (captureThread != null)
			return;

		captureThread = new Thread(new Runnable() {
			//body of capture thread
			public void run() {
				while (captureThread != null) {
					if (jpcap.processPacket(1, handler) == 0)
						//stopCaptureThread();
						Thread.yield();
				}
				System.out.println("Exiting Capture Thread");
				jpcap.breakLoop();
			}
		});
		captureThread.setPriority(Thread.MIN_PRIORITY);
		captureThread.start();
	}

	void stopCaptureThread() {
		captureThread = null;
	}

	private PacketReceiver handler = new PacketReceiver() {
		public void receivePacket(Packet packet) {
			byte[] data = new byte[packet.data.length + packet.header.length];
			System.arraycopy(packet.header, 0, data, 0, packet.header.length);
			System.arraycopy(packet.data, 0, data, packet.header.length,
					packet.data.length);
			EtherealPacket ePacket = new EtherealPacket(data, packet.sec
					* 1000000 + packet.usec, packet.len);
			addPacket(ePacket);
		}
	};

	public static void main(String[] args) {
		new Ethereal().init();
	}

	public void windowActivated(WindowEvent e) {
	}

	public void windowClosed(WindowEvent e) {
		stopCaptureThread();
	}

	public void windowClosing(WindowEvent e) {
	}

	public void windowDeactivated(WindowEvent e) {
	}

	public void windowDeiconified(WindowEvent e) {
	}

	public void windowIconified(WindowEvent e) {
	}

	public void windowOpened(WindowEvent e) {
	}

}

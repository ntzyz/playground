using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;
using System.Threading;
using System.Windows.Forms;
using System.IO;

namespace Daemon
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private bool isJavaRunning = false;
        Process javaProcess = null;
        Thread javaProcessOutputHandler = null;
        private NotifyIcon icon;

        public MainWindow()
        {
            InitializeComponent();
            addIcon();

            try
            {
                using (StreamReader sr = new StreamReader("config.txt"))
                {
                    while (!sr.EndOfStream)
                    {
                        string line = sr.ReadLine();
                        int spliter = line.IndexOf('=');
                        if (spliter >= 0)
                        {
                            string key = line.Substring(0, spliter);
                            string value = line.Substring(spliter + 1);
                            switch (key)
                            {
                                case "nginxCommand":
                                    nginxCommand.Text = value;
                                    break;
                                case "nginxArguments":
                                    nginxArguments.Text = value;
                                    break;
                                case "javaCommand":
                                    javaCommand.Text = value;
                                    break;
                                case "javaArguments":
                                    javaArguments.Text = value;
                                    break;
                            }
                        }
                    }
                }
            }
            catch (Exception e) { }
        }

        private void alert(string message)
        {
            icon.BalloonTipText = message;
            icon.BalloonTipIcon = ToolTipIcon.Info;
            icon.BalloonTipTitle = "Alert!";
            icon.ShowBalloonTip(500);
        }

        private void addIcon()
        {
            icon = new NotifyIcon();
            icon.Text = "守护进程（大雾）";
            icon.Icon = Properties.Resources.icon;
            icon.Visible = true;
            icon.ContextMenu = new System.Windows.Forms.ContextMenu();
            icon.ContextMenu.MenuItems.Add("重启 Nginx", (s, e) => {
                runNginx();
            });
            icon.ContextMenu.MenuItems.Add("重启 Java", (s, e) => {
                if (isJavaRunning)
                {
                    abortJava();
                }
                runJava();
            });
            icon.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler((s, e) => {
                this.Show();
                this.Focus();
                this.WindowState = WindowState.Normal;
            });
        }

        private void runNginx()
        {
            var process = new Process();
            process.StartInfo.FileName = nginxCommand.Text;
            process.StartInfo.Arguments = nginxArguments.Text;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            process.StartInfo.CreateNoWindow = true;
            process.Start();
            process.WaitForExit();
            alert("Nginx 命令已执行");
        }

        private void runJava()
        {
            // Start the process
            javaProcess = new Process();
            javaProcess.StartInfo.FileName = javaCommand.Text;
            javaProcess.StartInfo.Arguments = javaArguments.Text;
            javaProcess.StartInfo.RedirectStandardOutput = true;
            javaProcess.StartInfo.UseShellExecute = false;
            javaProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            javaProcess.StartInfo.CreateNoWindow = true;
            javaProcess.Start();
            javaStdout.Text += "Process started.\r\n";
            btnJava.Content = "重启";
            btnJavaAbort.IsEnabled = true;

            // Change the running flag
            isJavaRunning = true;

            javaProcessOutputHandler = new Thread(javaStdoutHandler);
            javaProcessOutputHandler.Start();
            alert("Java 已启动");
        }

        private void abortJava()
        {
            // Already running, abort them.
            javaProcessOutputHandler.Abort();
            javaProcessOutputHandler = null;
            if (!javaProcess.HasExited)
                javaProcess.Kill();
            javaProcess = null;

            javaStdout.Text += "Process aborted.\r\n";
            isJavaRunning = false;
            btnJava.Content = "运行";
            btnJavaAbort.IsEnabled = false;
        }

        private void btnJava_Click(object sender, RoutedEventArgs e)
        {
            if (isJavaRunning)
            {
                abortJava();
            }
            runJava();
        }

        private void javaStdoutHandler()
        {
            while (!javaProcess.StandardOutput.EndOfStream)
            {
                string line = javaProcess.StandardOutput.ReadLine();
                Dispatcher.Invoke(() => {
                    javaStdout.Text += line + "\r\n";
                    javaStdout.ScrollToEnd();
                });
            }
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (isJavaRunning)
            {
                abortJava();
            }
            icon.Dispose();
            using (StreamWriter sw = new StreamWriter("config.txt"))
            {
                sw.WriteLine("nginxCommand=" + nginxCommand.Text);
                sw.WriteLine("nginxArguments=" + nginxArguments.Text);
                sw.WriteLine("javaCommand=" + javaCommand.Text);
                sw.WriteLine("javaArguments=" + javaArguments.Text);
            }
        }

        private void btnJavaAbort_Click(object sender, RoutedEventArgs e)
        {
            if (isJavaRunning)
            {
                abortJava();
            }
        }

        private void btnJavaLogClear_Click(object sender, RoutedEventArgs e)
        {
            javaStdout.Text = "";
        }

        private void btnNginx_Click(object sender, RoutedEventArgs e)
        {
            runNginx();
        }

        private void Window_StateChanged(object sender, EventArgs e)
        {
            if (this.WindowState == WindowState.Minimized)
            {
                this.Hide();
            }
        }
    }
}

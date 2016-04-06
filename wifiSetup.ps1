#Powershell script to set up wifi on Spark Core or Particle Photon

#entry dialog
function config ($com,$deviceID) {
[void][System.Reflection.Assembly]::LoadWithPartialName( “System.Windows.Forms”)
[void][System.Reflection.Assembly]::LoadWithPartialName( “Microsoft.VisualBasic”)

$form = New-Object “System.Windows.Forms.Form”;
$form.Width = 500;$form.Height = 180;$form.Text = ($com + “ Enter wifi Data”);
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen;

$textLabel0 = New-Object “System.Windows.Forms.Label”;
$textLabel0.Left = 25;$textLabel0.Top = 15;$textLabel0.Text = “DeviceID”;
$textBox0 = New-Object “System.Windows.Forms.TextBox”;
$textBox0.Left = 150;$textBox0.Top = 10;$textBox0.width = 200;
$textBox0.Text = $deviceID;$textBox0.ReadOnly = 1

$textLabel1 = New-Object “System.Windows.Forms.Label”;
$textLabel1.Left = 25;$textLabel1.Top = 50;$textLabel1.Text = “SSID”;
$textBox1 = New-Object “System.Windows.Forms.TextBox”;
$textBox1.Left = 150;$textBox1.Top = 45;$textBox1.width = 200;
$textBox1.Text = ""

$textLabel2 = New-Object “System.Windows.Forms.Label”;
$textLabel2.Left = 25;$textLabel2.Top = 80;$textLabel2.Text = “Password”;
$textBox2 = New-Object “System.Windows.Forms.TextBox”;
$textBox2.Left = 150;$textBox2.Top = 75;$textBox2.width = 200;
$textBox2.PasswordChar = "*"
$textBox2.Text = ""

$textLabel3 = New-Object “System.Windows.Forms.Label”;
$textLabel3.Left = 25;$textLabel3.Top = 115;$textLabel3.Text = “Security”;
$comboBox1 = New-Object “System.Windows.Forms.ComboBox”;
$comboBox1.Left = 150;$comboBox1.Top = 110;$comboBox1.width = 100;
$comboBox1.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
[void]$comboBox1.items.AddRange(("None","WEP","WPA","WPA2")); 
$comboBox1.SelectedIndex = 3

$button = New-Object “System.Windows.Forms.Button”;
$button.Left = 360;$button.Top = 110;$button.Width = 100;$button.Text = “Ok”;

$eventHandler = [System.EventHandler]{
$textBox1.Text;$textBox2.Text;#$comboBox1.SelectedIndex
$form.Close();};

$button.Add_Click($eventHandler) ;

$form.Controls.Add($button);
$form.Controls.Add($textLabel0);$form.Controls.Add($textBox0);
$form.Controls.Add($textLabel1);$form.Controls.Add($textBox1);
$form.Controls.Add($textLabel2);$form.Controls.Add($textBox2);
$form.Controls.Add($textLabel3);$form.Controls.Add($comboBox1);
$ret = $form.ShowDialog();

return $textBox1.Text, $textBox2.Text, $comboBox1.SelectedIndex
}

function getComport() {
   $ports =  Get-WMIObject Win32_SerialPort
   foreach ($port in $ports) {
      if ($port.Description -like "*photon*" -or $port.Description -like "*spark core*") {
         return $port.DeviceID
      }
   }
   return ""
}
function openComport($com) {
   $port.PortName = $com
   $port.BaudRate = "9600"
   $port.Parity = "None"
   $port.DataBits = 8
   $port.StopBits = 1
   $port.ReadTimeout = 9000 # 9 seconds
   $port.DtrEnable = "true"
   $port.Open()
}

function setWifi($com,$SSID,$Password, $Security) {
   $port.Write("w")
   Write-Host "Entering wifi setup"
   Start-Sleep -s 3
   Write-Host ("Sending SSID " + $SSID)
   $port.WriteLine($SSID)
   Start-Sleep -s 3
   Write-Host ("Sending Security type " + $Security)
   $port.WriteLine($Security)
   Start-Sleep -s 3
   Write-Host "Sending Password"
   $port.WriteLine($Password)
   Start-Sleep -s 7
   Write-Host "All done"
   Start-Sleep -s 3
 }
function getDeviceID($com) {
   Write-Host ("Send i to port " + $com)
   $port.Write("i")
   $reply = $port.ReadLine()
   $fields = $reply.split()
   Write-Host $reply
   $fields = $reply.trim().split()
   return $fields[-1];
}


$wsh = new-object -ComObject wscript.shell
$port = new-Object System.IO.Ports.SerialPort
$com = (getComport)

if ($com -ne "") {
   openComport $com
   $deviceID = getDeviceID($com)
   $wifi= config $com $deviceID 
   if ($wifi[0] -ne "") {
      setWifi $com $wifi[0] $wifi[1] $wifi[2]
   } else {
      Write-Host "cancel set up"
   }
   $port.Close()
} else {
   $wsh.Popup("No port found")
}

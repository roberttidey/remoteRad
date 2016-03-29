<?php
require_once("config.php");

function getDeviceId($deviceName)
{
   $file = file("devices.txt");
   foreach ($file as $line)
   {
      $device = explode(",",$line);
      if (count($device) >= 2 && $device[0] === $deviceName)
      {
         return trim($device[1]); 
      }
   }
   return false;
}

function doGet($deviceId, $cmd)
{
   $requestUrl = BASE_URL . "$deviceId/$cmd/?access_token=" . ACCESS_TOKEN;
   $json = @file_get_contents($requestUrl);
   if ($json !== false)
   {
      $response = json_decode($json, true);
      if (array_key_exists("result", $response))
      {
         return $response["result"];
      }
      else
      {
         return "No $cmd result found";
      }
   }
   else
   {
      return "No response to $cmd call";
   }
}

function doPost($deviceId, $cmd, $params)
{
   if (is_string($params))
   {
      $requestUrl = BASE_URL . "$deviceId/$cmd";
      $data = array("params" => $params, "access_token" => ACCESS_TOKEN);
      $options = array(
          "http" => array(
              "header"  => "Content-type: application/x-www-form-urlencoded\r\n",
              "method"  => 'POST',
              "content" => http_build_query($data)
          )
      );
      $context  = stream_context_create($options);
      $result = @file_get_contents($requestUrl, false, $context);
      if ($result)
      {
         return "Success";
      }
      else 
      {
         return "Command $cmd failed";
      }
   }
   else
   {
      return "Invalid parameters";
   }
}

function doSchedule($deviceId, $sch)
{
   if (is_string($sch))
   {
      $days = explode(":", $sch);
      foreach ($days as $day)
      {
         $result = doPost($deviceId, "receiveSch", $day);
         if ($result != "Success") 
         {
            break;
         }
      }
      return $result;
   }
   else
   {
      return "Invalid schedule";
   }
}

$deviceName = filter_input(INPUT_GET, "deviceName", FILTER_SANITIZE_STRING);
$deviceId = getDeviceId($deviceName);
if ($deviceId === false)
{
   $result = "Invalid device $deviceName";
}
else
{
   $cmd = filter_input(INPUT_GET, "cmd", FILTER_SANITIZE_STRING);
   switch ($cmd)
   {
      case "status":
      case "schedule":
         $result = doGet($deviceId, $cmd);
         break;
      case "receiveSch":
         $sch = filter_input(INPUT_POST, "params", FILTER_SANITIZE_STRING);
         $result = doSchedule($deviceId, $sch);
         break;
      case "sendMsg":
         $params = filter_input(INPUT_POST, "params", FILTER_SANITIZE_STRING);
         $result = doPost($deviceId, $cmd, $params);
         break;
      default:
         $result = "Bad command";
   }
}
      
echo $result;

?>
<?php
$bed = new SaeStorage();
$domain = 'lwqqbed';

if($_POST){
  $user = $_POST['user'];
  $file = $_FILES['name']['tmp_name'];
  $suffix = strstr($_FILES['name']['name'], '.');
  $time = $user.microtime(true);
  $dest = md5($user."$time").$suffix;
  $result = $bed->upload($domain, $dest, $file, -1);
  echo "$result";
}

if($_GET){
  $image = $bed->getCDNUrl($domain, $_GET['id']);
  header("Location: ".$image);
}

?>

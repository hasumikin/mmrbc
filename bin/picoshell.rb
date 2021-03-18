#!/usr/bin/env ruby

require "bundler"
Bundler.require

class Client
  def initialize(fd)
    @serialport = Serial.new(fd)
    begin
      stty_save = `stty -g`.chomp
    rescue
    end
    sleep 1
  end

  def close
    @serialport.close
  end

  def get_shell_pid
    while true
      sleep 0.1
      result = @serialport.read(1024)
      if result == ""
        return nil
      elsif data = result.match(/pid: (\d+)/)
        return data[1].to_i
      else
        # 読み捨て
      end
    end
  end
end

TMPDIR = Dir.tmpdir
PID_SOCAT = TMPDIR + "/picoshell_socat.pid"
SOCAT_OUTPUT = TMPDIR + "/socat_output"

begin
  `socat -d -d pty,raw,echo=0 pty,raw,echo=0 </dev/null > #{SOCAT_OUTPUT} 2>&1 & echo $! > #{PID_SOCAT}`
  socat_pid = `cat #{PID_SOCAT}`.to_i
  shell_pid = nil
  sleep 1
  File.open(SOCAT_OUTPUT, "r") do |f|
    fd_server = f.gets.split(" ").last
    fd_client = f.gets.split(" ").last
    puts "fd_server: #{fd_server}"
    puts "fd_client: #{fd_client}"
    systemu("../build/host-debug/bin/picoshell #{fd_server}") do |server_pid|
      puts "client pid: #{Process.pid}"
      puts "socat  pid: #{socat_pid}"
      puts "server pid: #{server_pid}"
      client = Client.new(fd_client)
      shell_pid = client.get_shell_pid
      client.close
      puts "shell  pid: #{shell_pid}"
      puts
      puts "exec command below on another terminal:"
      puts "% cu -s 115200 -l #{fd_client}"
      puts "(type ~. to exit client)"
      while true
        print "type any line to exit server > "
        gets
        break;
      end
      Process.kill 9, server_pid
    end
  end
ensure
  Process.kill 9, socat_pid
  Process.kill 9, shell_pid if shell_pid
end


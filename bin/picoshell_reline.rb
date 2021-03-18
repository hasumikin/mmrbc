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

  def start
    puts 'picoshell client started.'
    begin
      while line = Reline.readline("picoshell> ", true)
        case line.chomp
        when 'exit', 'quit'
          @serialport.write "exit"
          break
        when /\A\s*\z/
          # next
        else
          @serialport.write line
          sleep 0.1
          puts_result
        end
      end
    rescue Interrupt
      puts '^C'
      `stty #{stty_save}` if stty_save
      exit 0
    end
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

  def puts_result
    while true
      result = @serialport.read(1024)
      if result == ""
        break
      else
        puts result
        sleep 0.1
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
    systemu("../build/host-debug/bin/picoshell #{fd_server}") do |server_pid|
      puts "client pid: #{Process.pid}"
      puts "socat  pid: #{socat_pid}"
      puts "server pid: #{server_pid}"
      client = Client.new(fd_client)
      shell_pid = client.get_shell_pid
      puts "shell  pid: #{shell_pid}"
      client.start
      Process.kill 9, server_pid
    end
  end
ensure
  Process.kill 9, socat_pid
  Process.kill 9, shell_pid if shell_pid
end

puts "pid: #{pid}" # client will receive the pid
while true
  suspend_task # suspend task itself
  line = gets
  if line != nil && line.size > 0
    case line.chomp
    when "quit", "exit"
      exit_shell
    else
      unless compile_and_run(line)
        puts "Failed to compile!"
      end
    end
  end
end


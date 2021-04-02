# for a sample project of PSoC5LP
# https://github.com/hasumikin/mruby_machine_PSoC5LP
class LED
  def on
    led_on
  end
  def off
    led_off
  end
end

# abandon buffer
while !fd_empty? do
  c = getc
  print c
end

prompt = "mruby machine > "

if pid > 0
  # skip unless POSIX
  puts "pid: #{pid}" # client will receive the pid
end

MAX_HISTORY_SIZE = 10
$history = Array.new

line = "" # String.new does not work...?
print prompt

while true
  suspend_task # suspend task itself
  while !fd_empty? do
    c = getc
    break if c == nil
    case c.ord
    when 7 # ESC
      exit_shell
    when 9 # horizontal tab \t
      # ignore
    when 10 # LF \n
      # ignore
    when 13  # CR \r
      print "\r\n"
      case line.chomp
      when ""
        # skip
      when "quit", "exit"
        exit_shell
      else
        if compile(line)
          $history.unshift(line) unless line == "$history";
          $history.pop if $history.size > MAX_HISTORY_SIZE
          result = execute_vm
          print "=> "
          p result
        else
          puts "syntax error"
        end
      end
      print prompt
      line = ""
    when 14, 15 # Shift Out, Shift In
      print "shift"
    when 127 # backspace
      if line.size > 0
        line = line[0, line.size - 1] # line.chop!
        print "\b \b"
      end
    when 27 # escape sequence?
      case getc
      when '['
        case gets
        when 'D'
          print "\033[D"
        end
      end

    else
      if 31 < c.ord && c.ord < 127
        print c
        line << c
      end
    end
  end
end

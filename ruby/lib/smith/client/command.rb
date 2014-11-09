# Base class for commands

module Smith
  module Client
    class Command

      include URLHelper
      
      def initialize(printer, state, http_client, payload)
        @printer, @state, @http_client, @payload = printer, state, http_client, payload
      end

      private 

      # Send an command acknowledgement post request state is the stage of the command acknowledgment
      # If only the state is specified, the message is nil
      # If a string is specified as the second argument, it is formatted as a log message using any additional arguments
      # If something other than a string is specified, it is used directly
      def acknowledge_command(state, *args)
        m = message(*args)
        request = @http_client.post(acknowledge_endpoint, command: @payload.command, command_token: @payload.command_token, state: state, message: m)
        request.callback { Client.log_debug(LogMessages::ACKNOWLEDGE_COMMAND, @payload.command, @payload.command_token, state, m) }
      end

      # Get the message portion of the acknowledgment according to the specified arguments
      def message(*args)
        return                          if args.empty?
        return LogMessage.format(*args) if args.first.is_a?(String)
        return args.first
      end

    end
  end
end

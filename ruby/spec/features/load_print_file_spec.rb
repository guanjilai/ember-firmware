require 'server_helper'

module Smith
  describe 'Load print file', :tmp_dir do
    include PrintEngineHelper
    include Rack::Test::Methods

    let(:print_file) { resource 'print.tar.gz' }
    let(:stale_print_file) { File.join(print_data_dir, 'old_print.tar.gz') }
    let(:uploaded_print_file) { File.join(print_data_dir, 'print.tar.gz') }
    let(:response_body) { JSON.parse(last_response.body, symbolize_names: true) }

    def assert_print_file_processed
      expect(File.read(uploaded_print_file)).to eq(File.read(print_file))
      expect(next_command_in_command_pipe).to eq(CMD_START_PRINT_DATA_LOAD)
      expect(next_command_in_command_pipe).to eq(CMD_PROCESS_PRINT_DATA)
    end

    context 'when communication via command pipe is possible' do

      before do
        create_command_pipe
        create_print_data_dir
        create_printer_status_file
        open_command_pipe
      end

      after do
        close_command_pipe
      end

      scenario 'user loads print file when printer is in Home state' do
        # Create a stale print file
        FileUtils.touch(stale_print_file)

        visit '/print_file_uploads/new'
        attach_file 'Select print file to load', print_file

        set_printer_status(state: HOME_STATE, ui_sub_state: NO_SUBSTATE)

        click_button 'Load'

        # Stale print files are removed
        expect(File.file?(stale_print_file)).to eq(false)
        expect(page).to have_content /Print file loaded successfully/i
        assert_print_file_processed   
      end

      scenario 'user loads print file via JSON request when printer is in Home state' do
        # Create a stale print file
        FileUtils.touch(stale_print_file)

        set_printer_status(state: HOME_STATE, ui_sub_state: NO_SUBSTATE)

        header 'Accept', 'application/json'
        post '/print_file_uploads', print_file: Rack::Test::UploadedFile.new(print_file)

        # Stale print files are removed
        expect(File.file?(stale_print_file)).to eq(false)
        expect(last_response.status).to eq(200)
        expect(response_body[:success]).to match(/Print file loaded successfully/i)
        assert_print_file_processed   
     end

      scenario 'user loads print file when printer is not in valid state' do
        visit '/print_file_uploads/new'
        attach_file 'Select print file to load', print_file

        set_printer_status(state: PRINTING_STATE, ui_sub_state: NO_SUBSTATE)

        click_button 'Load'

        expect(page).to have_content /Printer state \(state: "#{PRINTING_STATE}", ui_sub_state: "#{NO_SUBSTATE}"\) invalid/i
      end

      scenario 'user loads print file via JSON request when printer is not in valid state' do
        set_printer_status(state: PRINTING_STATE, ui_sub_state: NO_SUBSTATE)

        header 'Accept', 'application/json'
        post '/print_file_uploads', print_file: Rack::Test::UploadedFile.new(print_file)
        
        expect(last_response.status).to eq(500)
        expect(response_body[:error]).to match(/Printer state \(state: "#{PRINTING_STATE}", ui_sub_state: "#{NO_SUBSTATE}"\) invalid/i)
      end

    end

    scenario 'user loads print file when communication with print engine is not possible' do
      Settings.printer_status_file = 'does not exist'
      
      visit '/print_file_uploads/new'
      attach_file 'Select print file to load', print_file 
     
      click_button 'Load'
    
      expect(page).to have_content /Unable to read valid JSON from printer status file/i
    end
    
    scenario 'print file missing on upload' do
      visit '/print_file_uploads/new'

      click_button 'Load'
      
      expect(page).to have_content /Please select a print file/i
    end

    scenario 'print file is not a .tar.gz file' do
      visit '/print_file_uploads/new'
      attach_file 'Select print file to load', resource('smith-0.0.2-valid.tar')
      
      click_button 'Load'
      
      expect(page).to have_content /Please select a .tar.gz file/i
    end

    scenario 'user loads print file via JSON request when communication with print engine is not possible' do
      Settings.printer_status_file = 'does not exist'

      header 'Accept', 'application/json'
      post '/print_file_uploads', print_file: Rack::Test::UploadedFile.new(print_file)
      
      expect(last_response.status).to eq(500)
      expect(response_body[:error]).to match(/Unable to read valid JSON from printer status file/i)
    end
    
    scenario 'print file missing from JSON request' do
      header 'Accept', 'application/json'
      post '/print_file_uploads'
     
      expect(last_response.status).to eq(400)
      expect(response_body[:error]).to match(/Please select a print file/i)
    end

    scenario 'print file in JSON request is not a .tar.gz file' do
      header 'Accept', 'application/json'
      post '/print_file_uploads', print_file: Rack::Test::UploadedFile.new(resource('smith-0.0.2-valid.tar'))
      
      expect(last_response.status).to eq(400)
      expect(response_body[:error]).to match(/Please select a .tar.gz file/i)
    end
   
  end
end

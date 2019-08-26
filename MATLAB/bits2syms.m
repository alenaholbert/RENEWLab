%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%	Author(s): C. Nicolas Barati nicobarati@rice.edu 
%
%---------------------------------------------------------------------
% Original code copyright Mango Communications, Inc.
% Distributed under the WARP License http://warpproject.org/license
% Copyright (c) 2018-2019, Rice University
% RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license
% ---------------------------------------------------------------------
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


function [syms, data] = bits2syms(bitstream, MOD_ORDER)
% Returns modulated complex symbols and symbols in form of integer numbers

nbits = length(bitstream);
sym_bits = log2(MOD_ORDER);
if mod(nbits, sym_bits) ~= 0
    error("Length of bit stream has to be divisible by log2(MOD_ORDER)");
end

% bits to integer numbers
bit_mat = reshape(bitstream, sym_bits, [])';
data = bi2de(bit_mat, 'left-msb');

% Functions for data -> complex symbol mapping (like qammod, avoids comm toolbox requirement)
% These anonymous functions implement the modulation mapping from IEEE 802.11-2012 Section 18.3.5.8
modvec_bpsk   =  (1/sqrt(2))  .* [-1 1];
modvec_16qam  =  (1/sqrt(10)) .* [-3 -1 +3 +1];
modvec_64qam  =  (1/sqrt(42)) .* [-7 -5 -1 -3 +7 +5 +1 +3];

mod_fcn_bpsk  = @(x) complex(modvec_bpsk(1+x),0);
mod_fcn_qpsk  = @(x) complex(modvec_bpsk(1+bitshift(x, -1)), modvec_bpsk(1+mod(x, 2)));
mod_fcn_16qam = @(x) complex(modvec_16qam(1+bitshift(x, -2)), modvec_16qam(1+mod(x,4)));
mod_fcn_64qam = @(x) complex(modvec_64qam(1+bitshift(x, -3)), modvec_64qam(1+mod(x,8)));

% Map the data values on to complex symbols
switch MOD_ORDER
    case 2         % BPSK
        syms = arrayfun(mod_fcn_bpsk, data);
    case 4         % QPSK
        syms = arrayfun(mod_fcn_qpsk, data);
    case 16        % 16-QAM
        syms = arrayfun(mod_fcn_16qam, data);
    case 64        % 64-QAM
        syms = arrayfun(mod_fcn_64qam, data);
    otherwise
        fprintf('Invalid MOD_ORDER (%d)!  Must be in [2, 4, 16, 64]\n', MOD_ORDER);
        return;
end

end
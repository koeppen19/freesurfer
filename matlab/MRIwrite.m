function err = MRIwrite(mri,fstring)
% err = MRIwrite(mri,fstring)
%
% Writes an MRI volume based on the fstring. fstring can be:
%  1. MGH file. Eg, f.mgh or f.mgz
%  2. BHDR file Eg, f.bhdr. Result will be written to a bfloat
%     volume, eg f_000.bfloat.
%
% mri should be a structure like that read by MRIread.m The goemetry
% (ie, direction cosines, voxel resolution, and P0 are all recomputed
% from mri.vox2ras0. So, if in the course of analysis, you changed
% mri.x_r, this change will not be reflected in the output volume.
% 
%
% $Id: MRIwrite.m,v 1.2.2.1 2005/04/12 15:34:45 greve Exp $

err = 1;

if(nargin ~= 2)
  fprintf('err = MRIwrite(mri,fstring)\n');
  return;
end

[fspec fstem fmt] = MRIfspec(fstring,0); % 0 = turn off checkdisk
if(isempty(fspec))
  fprintf('ERROR: could not determine format of %s\n',fstring);
  return;
end

switch(fmt)
 case {'mgh','mgz'} %----------- MGH/MGZ ------------%
  M = mri.vox2ras0;
  mr_parms = [mri.tr mri.flip_angle mri.te mri.ti];
  err = save_mgh(permute(mri.vol,[2 1 3 4]), fspec, M, mr_parms);  
  return;
 case {'bhdr'} %----------- BHDR ------------%
  bmri.te = mri.te;
  bmri.tr = mri.tr/1000.0; % convert from msec to sec
  bmri.ti = mri.ti;
  bmri.flip_angle = mri.flip_angle;
  bmri.voldim = [size(mri.vol,1) size(mri.vol,2) size(mri.vol,3)];
  bmri.nframes = size(mri.vol,4);
  bmri.T = mri.vox2ras0;
  % Recompute voxel size based on vox2ras, to assure that
  % things only depend upon vox2ras0.
  xsize = sqrt(sum(mri.vox2ras0(:,1).^2)); 
  ysize = sqrt(sum(mri.vox2ras0(:,2).^2));
  zsize = sqrt(sum(mri.vox2ras0(:,3).^2));
  bmri.volres = [mri.xsize mri.ysize mri.zsize];
  err = fast_svbslice(mri.vol,fstem,[],'',bmri);
 otherwise
  fprintf('ERROR: format %s not supported\n',fmt);
  return;
end

return;












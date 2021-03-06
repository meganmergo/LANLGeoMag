#!/usr/bin/env python

'''Script to verify the Lstar calculation of LanlGeoMag

Uses the centred dipole to get the expected value'''

from __future__ import division
import matplotlib.pyplot as plt
import itertools

import lgmpy, copy
import spacepy.time as spt
import spacepy.coordinates as spc
import spacepy.datamodel as dm
import spacepy.toolbox as tb

def runQs():
    #make figure
    fig = plt.figure(figsize=(17, 21))
    
    #set up test runs
    nvals, rvals = 5, [2, 3, 4, 5, 6]
    dates = spt.tickrange('2001-04-22T12:00:00', '2001-04-23T23:00:00', 1/24)
    alpha = 45
    
    #loop over radial positions, dates and quality flags
    for rval in rvals:
        Lstar = [[]]*nvals
        for date, qual in itertools.product(dates.UTC, range(nvals)):
            pos = dm.dmarray([-1*rval, 0, 0], attrs={'sys': 'SM'})
            data = lgmpy.get_Lstar(pos, date, alpha=alpha, coord_system='SM', Bfield='Lgm_B_cdip', LstarThresh=20.0, extended_out=True, LstarQuality=qual)
            try:
                Lstar[qual].extend(data[alpha]['Lstar'])
            except TypeError:
                Lstar[qual].append(data[alpha]['Lstar'].tolist())
        print('Did [-%d,0,0] for all qualities' % rval)
        #make plots
        fstr = '%d1%d' % (len(rvals), rval-rvals[0]+1)
        ax = fig.add_subplot(fstr)
        ax.boxplot(Lstar)
        #ax = plt.gca()
        ax.set_title('LGM - Centred dipole [-%d, 0, 0]$_{SM}$; PA=%d$^{o}$' % (rval, alpha))
        ax.set_ylabel('L* (LanlGeoMag)')
        ax.set_xlabel('Quality Flag')
        ax.set_xticklabels([str(n) for n in range(5)])
        
        tb.savepickle('lgm_cdip_lstar%d_alpha%d.pkl' % (rval, alpha), {'Lstar': Lstar})
    
        plt.ylim([rval-0.04, rval+0.04])
        plt.savefig('lgm_cdip_verify%d.png' % alpha, dpi=300)
    ##fig.savefig('lgm_cdip_verify%d_zoom.png' % alpha, dpi=300)

def runPA(quality):
    #test for range of pitch angles
    ticks = spt.tickrange('2002-04-18', '2002-04-19', 1)
    loci = spc.Coords([[-4,0,0], [-5,0,0]], 'SM', 'car')
    vals = []
    for pp in range(1,91):
        dum = lgmpy.get_Lstar(loci.data[1], ticks.UTC[1], alpha=pp, coord_system='SM', 
                              Bfield='Lgm_B_cdip', extended_out=True, LstarQuality=quality)
        try:
            vals.extend(dum[pp]['Lstar'])
        except (IndexError, TypeError): #get_Lstar returns a 0-D nan in some cases...
            vals.extend([dum[pp]['Lstar'].tolist()])
    fig = plt.figure()
    expect = float(tb.hypot(loci.data[1]))
    plt.plot(100*(dm.dmarray(vals)-expect)/expect, drawstyle='steps-mid')
    plt.ylim([-0.01, 0.01])
    plt.xlabel('Pitch Angle')
    plt.ylabel('100*(L* - L*$_{exp}$)/L*$_{exp}$')
    plt.title('Cent. dipole [-5,0,0]$_{SM}$'+'; Quality ({0})'.format(str(quality)))
    figname = 'MagEphem_CDIP_test_q{0}'.format(str(quality))   #'LGM_L*_vs_PA_cdip_q{0}'.format(str(quality))
    plt.savefig(''.join([figname,'.png']), dpi=300)
    plt.savefig(''.join([figname,'.pdf']))

def dungeyLeq(r):
    b = (30500/14.474)**(1./3.)
    return r/(1 + 0.5*(r/b)**3)


def runPAdungey(quality):
    #test for range of pitch angles
    ticks = spt.tickrange('2002-04-18', '2002-04-19', 1)
    loci = spc.Coords([[-4,0,0], [-5,0,0]], 'SM', 'car')
    vals = []
    for pp in range(1,91):
        dum = lgmpy.get_Lstar(loci.data[1], ticks.UTC[1], alpha=pp, coord_system='SM', 
                              Bfield='Lgm_B_Dungey', extended_out=True, LstarQuality=quality)
        try:
            vals.extend(dum[pp]['Lstar'])
        except (IndexError, TypeError): #get_Lstar returns a 0-D nan in some cases...
            vals.extend([dum[pp]['Lstar'].tolist()])
    fig = plt.figure()
    expect = dungeyLeq(tb.hypot(loci.data[1]))
    #print(expect)
    plt.plot(100*(dm.dmarray(vals)-expect)/expect, drawstyle='steps-mid')
    plt.ylim([-0.01, 0.01])
    plt.xlabel('Pitch Angle')
    plt.ylabel('100*(L* - L*$_{exp}$)/L*$_{exp}$')
    plt.title('Dungey model [-5,0,0]$_{SM}$'+'; Quality ({0})'.format(str(quality)))
    figname = 'MagEphem_Dungey_test_q{0}'.format(str(quality))   #'LGM_L*_vs_PA_cdip_q{0}'.format(str(quality))
    plt.savefig(''.join([figname,'.png']), dpi=300)
    plt.savefig(''.join([figname,'.pdf']))

if __name__ == '__main__':
    for qq in [1,2,3,4,5,6]:
        runPA(qq)
        runPAdungey(qq)
    #runQs()

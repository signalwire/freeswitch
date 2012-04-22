load ../unittest/tinterp_prev.txt;
load ../unittest/tinterp_interp.txt;
load ../unittest/tinterp_next.txt;

clf;
plot(tinterp_prev(:,1), 20.0*log10(tinterp_prev(:,2)),";prev;")
hold on;
plot(tinterp_interp(:,1), 20.0*log10(tinterp_interp(:,2)),'g+-;interp;')
plot(tinterp_next(:,1), 20.0*log10(tinterp_next(:,2)),'ro-;next;')
hold off;
axis([0 pi 0 80])
